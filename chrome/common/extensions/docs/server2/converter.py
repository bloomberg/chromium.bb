#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Example run from the server2/ directory:
# $ converter.py ../static/ ../../api templates/articles/ templates/intros/
#   templates/public/ static/images/ -r

import json
import optparse
import os
import re
import shutil
import subprocess
import sys

sys.path.append(os.path.join(sys.path[0], os.pardir, os.pardir, os.pardir,
    os.pardir, os.pardir, 'tools'))
import json_comment_eater

from converter_html_parser import HandleDocFamily
from docs_server_utils import SanitizeAPIName

IGNORED_FILES = [
  # These are custom files.
  '404',
  'apps_api_index',
  'api_index',
  'experimental',
  'samples',
  'index',
  'devtools', # Has an intro, but marked as nodoc.
]

# These are mappings for APIs that have no intros. They are needed because the
# names of the JSON files do not give enough information on the actual API name.
CUSTOM_MAPPINGS = {
  'experimental_input_virtual_keyboard': 'experimental_input_virtualKeyboard',
  'experimental_system_info_cpu': 'experimental_systemInfo_cpu',
  'experimental_system_info_storage': 'experimental_systemInfo_storage',
  'input_ime': 'input_ime',
  'app_runtime': 'app_runtime',
  'app_window': 'app_window',
}

# These are the extension-only APIs that don't have explicit entries in
# _permission_features.json, so we can't programmatically figure out that it's
# extension-only.
# From api_page_generator.js:548.
EXTENSIONS_ONLY = [
  'browserAction',
  'extension',
  'input_ime',
  'fontSettings',
  'fileBrowserHandler',
  'omnibox',
  'pageAction',
  'scriptBadge',
  'windows',
  'devtools_inspectedWindow',
  'devtools_network',
  'devtools_panels',
  'experimental_devtools_audits',
  'experimental_devtools_console',
  'experimental_discovery',
  'experimental_infobars',
  'experimental_offscreenTabs',
  'experimental_processes',
  'experimental_speechInput'
]

def _ReadFile(filename):
  with open(filename, 'r') as f:
    return f.read()

def _WriteFile(filename, data):
  with open(filename, 'w+') as f:
    f.write(data)

def _UnixName(name):
  """Returns the unix_style name for a given lowerCamelCase string.
  Shamelessly stolen from json_schema_compiler/model.py.
  """
  name = os.path.splitext(name)[0]
  s1 = re.sub('([a-z])([A-Z])', r'\1_\2', name)
  s2 = re.sub('([A-Z]+)([A-Z][a-z])', r'\1_\2', s1)
  return s2.replace('.', '_').lower()

def _GetDestinations(api_name, api_dir):
  if api_name in EXTENSIONS_ONLY:
    return ['extensions']
  if api_name.count('app') > 0:
    return ['apps']
  with open(os.path.join(api_dir, '_permission_features.json')) as f:
    permissions = json.loads(json_comment_eater.Nom(f.read()))
  permissions_key = api_name.replace('experimental_', '').replace('_', '.')
  if permissions_key in permissions:
    return_list = []
    types = permissions[permissions_key]['extension_types']
    if 'platform_app' in types:
      return_list.append('apps')
    if 'extension' in types:
      return_list.append('extensions')
    return return_list
  return ['extensions', 'apps']

def _ListAllAPIs(dirname):
  all_files = []
  for path, dirs, files in os.walk(dirname):
    if path == '.':
      all_files.extend([f for f in files
                        if f.endswith('.json') or f.endswith('.idl')])
    else:
      all_files.extend([os.path.join(path, f) for f in files
                        if f.endswith('.json') or f.endswith('.idl')])
  dirname = dirname.rstrip('/') + '/'
  return [f[len(dirname):] for f in all_files
          if os.path.splitext(f)[0].split('_')[-1] not in ['private',
                                                           'internal']]

def _MakeArticleTemplate(filename, path):
  return ('{{+partials.standard_%s_article article:intros.%s}}\n' %
          (path, filename))

def _MakeAPITemplate(intro_name, path, api_name, has_intro):
  if has_intro:
    return ('{{+partials.standard_%s_api api:apis.%s intro:intros.%s}}\n' %
            (path, api_name, intro_name))
  else:
    return ('{{+partials.standard_%s_api api:apis.%s}}\n' %
            (path, api_name))

def _GetAPIPath(name, api_dir):
  api_files = _ListAllAPIs(api_dir)
  for filename in api_files:
    if name == _UnixName(SanitizeAPIName(filename)):
      return _UnixName(filename)

def _CleanAPIs(source_dir, api_dir, intros_dest, template_dest, exclude):
  source_files = os.listdir(source_dir)
  source_files_unix = set(_UnixName(f) for f in source_files)
  api_files = set(_UnixName(SanitizeAPIName(f)) for f in _ListAllAPIs(api_dir))
  intros_files = set(_UnixName(f) for f in os.listdir(intros_dest))
  to_delete = api_files - source_files_unix - set(exclude)
  for path, dirs, files in os.walk(template_dest):
    for filename in files:
      no_ext = os.path.splitext(filename)[0]
      # Check for changes like appWindow -> app.window.
      if (_UnixName(filename) in source_files_unix - set(exclude) and
          no_ext not in source_files):
        os.remove(os.path.join(path, filename))
      if _UnixName(filename) in to_delete:
        try:
          os.remove(os.path.join(intros_dest, filename))
        except OSError:
          pass
        os.remove(os.path.join(path, filename))

def _FormatFile(contents, path, name, image_dest, replace, is_api):
  # Copy all images referenced in the page.
  for image in re.findall(r'="\.\./images/([^"]*)"', contents):
    if not replace and os.path.exists(os.path.join(image_dest, image)):
      continue
    if '/' in image:
      try:
        os.makedirs(os.path.join(image_dest, image.rsplit('/', 1)[0]))
      except:
        pass
    shutil.copy(
        os.path.join(path, os.pardir, 'images', image),
        os.path.join(image_dest, image))
  contents = re.sub(r'<!--.*?(BEGIN|END).*?-->', r'', contents)
  contents = re.sub(r'\.\./images', r'{{static}}/images', contents)
  exp = re.compile(r'<div[^>.]*?id="pageData-showTOC"[^>.]*?>.*?</div>',
                   flags=re.DOTALL)
  contents = re.sub(exp, r'', contents)
  exp = re.compile(r'<div[^>.]*?id="pageData-name"[^>.]*?>(.*?)</div>',
                   flags=re.DOTALL)
  if is_api:
    contents = re.sub(exp, r'', contents)
  else:
    contents = re.sub(exp, r'<h1>\1</h1>', contents)
  contents = contents.strip()
  contents = HandleDocFamily(contents)

  # Attempt to guess if the page has no title.
  if '<h1' not in contents and not is_api:
    title = _UnixName(name)
    title = ' '.join([part[0].upper() + part[1:] for part in title.split('_')])
    contents = ('<h1 class="page_title">%s</h1>' % title) + contents
  return contents

def _GetNoDocs(api_dir, api_files):
  exclude = []
  for api in api_files:
    try:
      with open(os.path.join(api_dir, api), 'r') as f:
        if os.path.splitext(api)[-1] == '.idl':
          if '[nodoc] namespace' in f.read():
            exclude.append(_UnixName(api))
        else:
          api_json = json.loads(json_comment_eater.Nom(f.read()))
          if api_json[0].get('nodoc', False):
            exclude.append(_UnixName(api))
    except Exception:
      pass
  return exclude

def _ProcessName(name):
  processed_name = []
  if name.startswith('experimental_'):
    name = name[len('experimental_'):]
    processed_name.append('experimental_')
  parts = name.split('_')
  processed_name.append(parts[0])
  processed_name.extend([p[0].upper() + p[1:] for p in parts[1:]])
  return ''.join(processed_name)

def _MoveAllFiles(source_dir,
                  api_dir,
                  articles_dest,
                  intros_dest,
                  template_dest,
                  image_dest,
                  replace=False,
                  exclude_dir=None,
                  svn=False):
  if exclude_dir is None:
    exclude_files = []
  else:
    exclude_files = [_UnixName(f) for f in os.listdir(exclude_dir)]
  exclude_files.extend(IGNORED_FILES)
  api_files = _ListAllAPIs(api_dir)
  original_files = []
  for path, dirs, files in os.walk(template_dest):
    original_files.extend(files)
  if replace:
    _CleanAPIs(source_dir, api_dir, intros_dest, template_dest, exclude_files)
  exclude_files.extend(_GetNoDocs(api_dir, api_files))
  files = set(os.listdir(source_dir))
  unix_files = [_UnixName(f) for f in files]
  for name in  [SanitizeAPIName(f)  for f in _ListAllAPIs(api_dir)]:
    if _UnixName(name) not in unix_files:
      files.add(name + '.html')
  for file_ in files:
    if (_UnixName(file_) in exclude_files or
        file_.startswith('.') or
        file_.startswith('_')):
      continue
    _MoveSingleFile(source_dir,
                    file_,
                    api_dir,
                    articles_dest,
                    intros_dest,
                    template_dest,
                    image_dest,
                    replace=replace,
                    svn=svn,
                    original_files=original_files)

def _MoveSingleFile(source_dir,
                    source_file,
                    api_dir,
                    articles_dest,
                    intros_dest,
                    template_dest,
                    image_dest,
                    replace=False,
                    svn=False,
                    original_files=None):
  unix_name = _UnixName(source_file)
  is_api = unix_name in [_UnixName(SanitizeAPIName(f))
                         for f in _ListAllAPIs(api_dir)]
  if unix_name in CUSTOM_MAPPINGS:
    processed_name = CUSTOM_MAPPINGS[unix_name]
  else:
    processed_name = os.path.splitext(source_file)[0].replace('.', '_')
    if (is_api and
        '_' in source_file.replace('experimental_', '') and
        not os.path.exists(os.path.join(source_dir, source_file))):
      processed_name = _ProcessName(processed_name)
      if (original_files is None or
          processed_name + '.html' not in original_files):
        print 'WARNING: The correct name of this file was guessed:'
        print
        print '%s -> %s' % (os.path.splitext(source_file)[0], processed_name)
        print
        print ('If this is incorrect, change |CUSTOM_MAPPINGS| in chrome/'
               'common/extensions/docs/server2/converter.py.')
  try:
    static_data = _FormatFile(_ReadFile(os.path.join(source_dir, source_file)),
                              source_dir,
                              source_file,
                              image_dest,
                              replace,
                              is_api)
  except IOError:
    static_data = None
  for path in _GetDestinations(processed_name, api_dir):
    template_file = os.path.join(template_dest, path, processed_name + '.html')
    if is_api:
      template_data = _MakeAPITemplate(processed_name,
                                       path,
                                       _GetAPIPath(unix_name, api_dir),
                                       static_data is not None)
      static_file = os.path.join(intros_dest, processed_name + '.html')
    else:
      template_data = _MakeArticleTemplate(processed_name, path)
      static_file = os.path.join(articles_dest, processed_name + '.html')
    if replace or not os.path.exists(template_file):
      _WriteFile(template_file, template_data)
      if svn:
        subprocess.call(['svn',
                         'add',
                         '--force',
                         '--parents',
                         '-q',
                         template_file])
  if static_data is not None and (replace or not os.path.exists(static_file)):
    if svn:
      if os.path.exists(static_file):
        subprocess.call(['svn', 'rm', '--force', static_file])
      subprocess.call(['svn',
                       'cp',
                       os.path.join(source_dir, source_file),
                       static_file])
    _WriteFile(static_file, static_data)

if __name__ == '__main__':
  parser = optparse.OptionParser(
      description='Converts static files from the old documentation system to '
                  'the new one. If run without -f, all the files in |src| will '
                  'be converted.',
      usage='usage: %prog [options] static_src_dir [-f static_src_file] '
            'api_dir articles_dest intros_dest template_dest image_dest')
  parser.add_option('-f',
                    '--file',
                    action='store_true',
                    default=False,
                    help='convert single file')
  parser.add_option('-s',
                    '--svn',
                    action='store_true',
                    default=False,
                    help='use svn copy to copy intro files')
  parser.add_option('-e',
                    '--exclude',
                    default=None,
                    help='exclude files matching the names in this dir')
  parser.add_option('-r',
                    '--replace',
                    action='store_true',
                    default=False,
                    help='replace existing files')
  (opts, args) = parser.parse_args()
  if (not opts.file and len(args) != 6) or (opts.file and len(args) != 7):
    parser.error('incorrect number of arguments.')

  if opts.file:
    _MoveSingleFile(*args, replace=opts.replace, svn=opts.svn)
  else:
    _MoveAllFiles(*args,
                  replace=opts.replace,
                  exclude_dir=opts.exclude,
                  svn=opts.svn)
