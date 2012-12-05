#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import re

from docs_server_utils import SanitizeAPIName
import third_party.json_schema_compiler.model as model

def _ReadFile(filename):
  with open(filename) as f:
    return f.read()

def _WriteFile(filename, contents):
  with open(filename, 'w') as f:
    f.write(contents)

def _Replace(matches, filename):
  if matches.group(2).count('#') != 1:
    return '<a%shref=%s>%s</a>' % (matches.group(1),
                                   matches.group(2),
                                   matches.group(3))
  clean = (matches.group(2).replace('\\', '')
                           .replace("'", '')
                           .replace('"', '')
                           .replace('/', ''))
  page, link = clean.split('#')
  if not page:
    page = '%s.html' % SanitizeAPIName(filename.rsplit(os.sep, 1)[-1])
  if (not link.startswith('property-') and
      not link.startswith('type-') and
      not link.startswith('method-') and
      not link.startswith('event-')):
    return '<a%shref=%s>%s</a>' % (matches.group(1),
                                   matches.group(2),
                                   matches.group(3))

  link = re.sub('^(property|type|method|event)-', '', link).replace('-', '.')
  page = page.replace('.html', '.').replace('_', '.')
  if matches.group(1) == ' ':
    padding = ''
  else:
    padding = matches.group(1)
  return '%s$ref:%s%s' % (padding, page, link)

if __name__ == '__main__':
  for root, dirs, files in os.walk(sys.argv[1]):
    for name in files:
      filename = os.path.join(root, name)
      contents = _ReadFile(filename)
      contents  = re.sub(r'<a(.*?)href=(.*?)>(.*?)</a>',
                         lambda m: _Replace(m, filename),
                         contents,
                         flags=re.DOTALL)
      contents = contents.replace('$ref:extension.lastError',
                                  '$ref:runtime.lastError')
      _WriteFile(filename, contents)
