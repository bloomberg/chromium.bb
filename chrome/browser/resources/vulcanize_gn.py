#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import itertools
import os
import platform
import re
import subprocess
import sys
import tempfile


_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..'))
_CWD = os.getcwd()

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
  '--exclude', 'crisper.js',

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


_REQUEST_LIST_FILE = 'request_list.txt'


_PAK_UNPACK_FOLDER = 'flattened'


def _run_node(cmd_parts, stdout=None):
  cmd = " ".join([node.GetBinaryPath()] + cmd_parts)
  process = subprocess.Popen(
      cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
  stdout, stderr = process.communicate()

  if stderr:
    print >> sys.stderr, '%s failed: %s' % (cmd, stderr)
    raise

  return stdout


def _undo_mapping(mappings, url):
  for (redirect_url, file_path) in mappings:
    if url.startswith(redirect_url):
      return url.replace(redirect_url, file_path + os.sep)
  return url


# Get a list of all files that were bundled with Vulcanize and update the
# depfile accordingly such that Ninja knows when to trigger re-vulcanization.
def _update_dep_file(in_folder, args):
  in_path = os.path.join(_CWD, in_folder)
  out_path = os.path.join(_CWD, args.out_folder)

  # Prior call to vulcanize already generated the deps list, grab it from there.
  request_list = open(os.path.join(
      out_path, _REQUEST_LIST_FILE), 'r').read().splitlines()

  # Undo the URL mappings applied by vulcanize to get file paths relative to
  # current working directory.
  url_mappings = _URL_MAPPINGS + [
      ('/', os.path.relpath(in_path, _CWD)),
      ('chrome://%s/' % args.host, os.path.relpath(in_path, _CWD)),
  ]

  dependencies = map(
      lambda url: _undo_mapping(url_mappings, url), request_list)

  # If the input was a .pak file, the generated depfile should not list files
  # already in the .pak file.
  filtered_dependencies = dependencies
  if (args.input_type == 'PAK_FILE'):
    filter_url = os.path.join(args.out_folder, _PAK_UNPACK_FOLDER)
    filtered_dependencies = filter(
        lambda url: not url.startswith(filter_url), dependencies)

  with open(os.path.join(_CWD, args.depfile), 'w') as f:
    f.write(os.path.join(
        args.out_folder, args.html_out_file) + ': ' + ' '.join(
            filtered_dependencies))


def _vulcanize(in_folder, out_folder, host, html_in_file,
               html_out_file, js_out_file):
  in_path = os.path.normpath(os.path.join(_CWD, in_folder))
  out_path = os.path.join(_CWD, out_folder)

  html_out_path = os.path.join(out_path, html_out_file)
  js_out_path = os.path.join(out_path, js_out_file)

  output = _run_node(
      [node_modules.PathToVulcanize()] +
      _VULCANIZE_BASE_ARGS + _VULCANIZE_REDIRECT_ARGS +
      ['--out-request-list', os.path.join(out_path, _REQUEST_LIST_FILE),
       '--redirect', '"/|%s"' % in_path,
       '--redirect', '"chrome://%s/|%s"' % (host, in_path),
       # TODO(dpapad): Figure out why vulcanize treats the input path
       # differently on Windows VS Linux/Mac.
       os.path.join(
           in_path if platform.system() == 'Windows' else os.sep,
           html_in_file)])

  with tempfile.NamedTemporaryFile(mode='wt+', delete=False) as tmp:
    # Grit includes are not supported, use HTML imports instead.
    tmp.write(output.replace(
        '<include src="', '<include src-disabled="'))

  try:
    _run_node([node_modules.PathToCrisper(),
             '--source', tmp.name,
             '--script-in-head', 'false',
             '--html', html_out_path,
             '--js', js_out_path])

    # TODO(tsergeant): Remove when JS resources are minified by default:
    # crbug.com/619091.
    _run_node([node_modules.PathToUglifyJs(), js_out_path,
              '--comments', '"/Copyright|license|LICENSE|\<\/?if/"',
              '--output', js_out_path])
  finally:
    os.remove(tmp.name)


def _css_build(out_folder, files):
  out_path = os.path.join(_CWD, out_folder)
  paths = map(lambda f: os.path.join(out_path, f), files)

  _run_node([node_modules.PathToPolymerCssBuild()] + paths)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--depfile')
  parser.add_argument('--host')
  parser.add_argument('--html_in_file')
  parser.add_argument('--html_out_file')
  parser.add_argument('--input')
  parser.add_argument('--input_type')
  parser.add_argument('--js_out_file')
  parser.add_argument('--out_folder')
  args = parser.parse_args()
  args.input = os.path.normpath(args.input)

  vulcanize_input_folder = args.input

  # If a .pak file was specified, unpack that file first and pass the output to
  # vulcanize.
  if (args.input_type == 'PAK_FILE'):
    import unpack_pak
    input_folder = os.path.join(_CWD, args.input)
    output_folder = os.path.join(args.out_folder, _PAK_UNPACK_FOLDER)
    unpack_pak.unpack(args.input, output_folder)
    vulcanize_input_folder = output_folder

  _vulcanize(vulcanize_input_folder, args.out_folder, args.host,
             args.html_in_file, args.html_out_file, args.js_out_file)
  _css_build(args.out_folder, files=[args.html_out_file])

  _update_dep_file(vulcanize_input_folder, args)


if __name__ == '__main__':
  main()
