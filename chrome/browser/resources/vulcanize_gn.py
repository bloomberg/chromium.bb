#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import itertools
import os
import platform
import re
import sys
import tempfile


_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..'))
_CWD = os.getcwd()  # NOTE(dbeam): this is typically out/<gn_name>/.

sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'node'))
import node
import node_modules


_RESOURCES_PATH = os.path.join(_SRC_PATH, 'ui', 'webui', 'resources')


_CR_ELEMENTS_PATH = os.path.join(_RESOURCES_PATH, 'cr_elements')


_CSS_RESOURCES_PATH = os.path.join(_RESOURCES_PATH, 'css')


_HTML_RESOURCES_PATH = os.path.join(_RESOURCES_PATH, 'html')


_JS_RESOURCES_PATH = os.path.join(_RESOURCES_PATH, 'js')


_POLYMER_PATH = os.path.join(
    _SRC_PATH, 'third_party', 'polymer', 'v1_0', 'components-chromium')


_VULCANIZE_BASE_ARGS = [
  # These files are already combined and minified.
  '--exclude', 'chrome://resources/html/polymer.html',
  '--exclude', 'web-animations-next-lite.min.js',

  # These files are dynamically created by C++.
  '--exclude', 'load_time_data.js',
  '--exclude', 'strings.js',
  '--exclude', 'text_defaults.css',
  '--exclude', 'text_defaults_md.css',

  '--inline-css',
  '--inline-scripts',
  '--strip-comments',
]


_URL_MAPPINGS = [
    ('chrome://resources/cr_elements/', _CR_ELEMENTS_PATH),
    ('chrome://resources/css/', _CSS_RESOURCES_PATH),
    ('chrome://resources/html/', _HTML_RESOURCES_PATH),
    ('chrome://resources/js/', _JS_RESOURCES_PATH),
    ('chrome://resources/polymer/v1_0/', _POLYMER_PATH)
]


_VULCANIZE_REDIRECT_ARGS = list(itertools.chain.from_iterable(map(
    lambda m: ['--redirect', '"%s|%s"' % (m[0], m[1])], _URL_MAPPINGS)))


_PAK_UNPACK_FOLDER = 'flattened'


def _undo_mapping(mappings, url):
  for (redirect_url, file_path) in mappings:
    if url.startswith(redirect_url):
      return url.replace(redirect_url, file_path + os.sep)
  # TODO(dbeam): can we make this stricter?
  return url

def _request_list_path(out_path, html_out_file):
  return os.path.join(out_path, html_out_file + '_requestlist.txt')

# Get a list of all files that were bundled with Vulcanize and update the
# depfile accordingly such that Ninja knows when to trigger re-vulcanization.
def _update_dep_file(in_folder, args):
  in_path = os.path.join(_CWD, in_folder)
  out_path = os.path.join(_CWD, args.out_folder)

  # Prior call to vulcanize already generated the deps list, grab it from there.
  request_list_path = _request_list_path(out_path, args.html_out_file)
  request_list = open(request_list_path, 'r').read().splitlines()

  if platform.system() == 'Windows':
    # TODO(dbeam): UGH. For some reason Vulcanize is interpreting the target
    # file path as a URL and using the drive letter (e.g. D:\) as a protocol.
    # This is a little insane, but we're fixing here by normalizing case (which
    # really shouldn't matter, these are all file paths and generally are all
    # lower case) and writing from / to \ (file path) and then back again. This
    # is compounded by NodeJS having a bug in url.resolve() that handles
    # chrome:// protocol URLs poorly as well as us using startswith() to strip
    # file paths (which isn't crazy awesome either). Don't remove unless you
    # really really know what you're doing.
    norm = lambda u: u.lower().replace('/', '\\')
    request_list = [norm(u).replace(norm(in_path), '').replace('\\', '/')
        for u in request_list]

  # Undo the URL mappings applied by vulcanize to get file paths relative to
  # current working directory.
  url_mappings = _URL_MAPPINGS + [
      ('/', os.path.relpath(in_path, _CWD)),
      ('chrome://%s/' % args.host, os.path.relpath(in_path, _CWD)),
  ]

  deps = [_undo_mapping(url_mappings, u) for u in request_list]
  deps = map(os.path.normpath, deps)

  # If the input was a .pak file, the generated depfile should not list files
  # already in the .pak file.
  if args.input.endswith('.pak'):
    filter_url = os.path.join(args.out_folder, _PAK_UNPACK_FOLDER)
    deps = [d for d in deps if not d.startswith(filter_url)]

  with open(os.path.join(_CWD, args.depfile), 'w') as f:
    deps_file_header = os.path.join(args.out_folder, args.html_out_file)
    f.write(deps_file_header + ': ' + ' '.join(deps))


def _vulcanize(in_folder, args):
  in_path = os.path.normpath(os.path.join(_CWD, in_folder))
  out_path = os.path.join(_CWD, args.out_folder)

  html_out_path = os.path.join(out_path, args.html_out_file)
  js_out_path = os.path.join(out_path, args.js_out_file)

  exclude_args = []
  for f in args.exclude or []:
    exclude_args.append('--exclude')
    exclude_args.append(f)

  output = node.RunNode(
      [node_modules.PathToVulcanize()] +
      _VULCANIZE_BASE_ARGS + _VULCANIZE_REDIRECT_ARGS + exclude_args +
      ['--out-request-list', _request_list_path(out_path, args.html_out_file),
       '--redirect', '"/|%s"' % in_path,
       '--redirect', '"chrome://%s/|%s"' % (args.host, in_path),
       # TODO(dpapad): Figure out why vulcanize treats the input path
       # differently on Windows VS Linux/Mac.
       os.path.join(
           in_path if platform.system() == 'Windows' else os.sep,
           args.html_in_file)])

  # Grit includes are not supported, use HTML imports instead.
  output = output.replace('<include src="', '<include src-disabled="')

  if args.insert_in_head:
    assert '<head>' in output
    # NOTE(dbeam): Vulcanize eats <base> tags after processing. This undoes
    # that by adding a <base> tag to the (post-processed) generated output.
    output = output.replace('<head>', '<head>' + args.insert_in_head)

  with tempfile.NamedTemporaryFile(mode='wt+', delete=False) as tmp:
    tmp.write(output)

  try:
    node.RunNode([node_modules.PathToCrisper(),
                 '--source', tmp.name,
                 '--script-in-head', 'false',
                 '--html', html_out_path,
                 '--js', js_out_path])

    node.RunNode([node_modules.PathToUglifyJs(), js_out_path,
                  '--comments', '"/Copyright|license|LICENSE|\<\/?if/"',
                  '--output', js_out_path])
  finally:
    os.remove(tmp.name)


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--depfile', required=True)
  parser.add_argument('--exclude', nargs='*')
  parser.add_argument('--host', required=True)
  parser.add_argument('--html_in_file', required=True)
  parser.add_argument('--html_out_file', required=True)
  parser.add_argument('--input', required=True)
  parser.add_argument('--insert_in_head')
  parser.add_argument('--js_out_file', required=True)
  parser.add_argument('--out_folder', required=True)
  args = parser.parse_args(argv)

  # NOTE(dbeam): on Windows, GN can send dirs/like/this. When joined, you might
  # get dirs/like/this\file.txt. This looks odd to windows. Normalize to right
  # the slashes.
  args.depfile = os.path.normpath(args.depfile)
  args.input = os.path.normpath(args.input)
  args.out_folder = os.path.normpath(args.out_folder)

  vulcanize_input_folder = args.input

  # If a .pak file was specified, unpack that file first and pass the output to
  # vulcanize.
  if args.input.endswith('.pak'):
    import unpack_pak
    output_folder = os.path.join(args.out_folder, _PAK_UNPACK_FOLDER)
    unpack_pak.unpack(args.input, output_folder)
    vulcanize_input_folder = output_folder

  _vulcanize(vulcanize_input_folder, args)

  _update_dep_file(vulcanize_input_folder, args)


if __name__ == '__main__':
  main(sys.argv[1:])
