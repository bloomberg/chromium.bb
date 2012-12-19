#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Embeds standalone JavaScript snippets in C++ code.

Each argument to the script must be a file containing an associated JavaScript
function (e.g., evaluate_script.js should contain an evaluateScript function).
This is called the exported function of the script. The entire script will be
put into a C-style string in the form of an anonymous function which invokes
the exported function when called.
"""

import optparse
import os
import sys


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--directory', type='string', default='.',
      help='Path to directory where the cc/h js file should be created')
  options, args = parser.parse_args()

  scripts = {}
  for js_file in args:
    base_name = os.path.basename(js_file)[:-3].title().replace('_', '')
    func_name = base_name[0].lower() + base_name[1:]
    script_name = 'k%sScript' % base_name
    with open(js_file, 'r') as f:
      contents = f.read()
    script = 'function() { %s; return %s.apply(null, arguments) }' % (
        contents, func_name)
    scripts[script_name] = script

  copyright = '\n'.join([
      '// Copyright (c) 2012 The Chromium Authors. All rights reserved.',
      '// Use of this source code is governed by a BSD-style license that '
          'can be',
      '// found in the LICENSE file.'])

  # Write header file.
  externs = []
  for name in scripts.iterkeys():
    externs += ['extern const char %s[];' % name]
  header = '\n'.join([
      copyright,
      '',
      '#ifndef CHROME_TEST_CHROMEDRIVER_JS_H_',
      '#define CHROME_TEST_CHROMEDRIVER_JS_H_',
      '#pragma once',
      '',
      '\n'.join(externs),
      '',
      '#endif  // CHROME_TEST_CHROMEDRIVER_JS_H_'])

  with open(os.path.join(options.directory, 'js.h'), 'w') as f:
    f.write(header)

  # Write cc file.
  declarations = []
  for name, script in scripts.iteritems():
    lines = []
    for line in script.split('\n'):
      lines += ['    "%s\\n"' % line.replace('"', '\\"')]
    declarations += ['const char %s[] = \n%s;' % (name, '\n'.join(lines))]
  cc = '\n'.join([
      copyright,
      '',
      '#include "chrome/test/chromedriver/js.h"',
      '',
      '\n'.join(declarations)])

  with open(os.path.join(options.directory, 'js.cc'), 'w') as f:
    f.write(cc)


if __name__ == '__main__':
  sys.exit(main())
