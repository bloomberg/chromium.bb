#!/usr/bin/env python
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a stripped down version of a java factory file.

A stripped down factory file is required in a feature's public_java target
during the compilation process so that features can depend on each other
without creating circular dependencies.

Afterwards, the stripped down factory's .class file is excluded from the
resulting target. The real factory uses the feature's internal implementations,
which is why it is not included in the feature's public_java target.

This script generates stripped down factory files from real factory files to
reduce the burden of maintenance. It checks to make sure that these factory
files are as simple as possible and replaces multi-line java return statements
with 'return null;'.

In order to keep the regex simple and easily understood, the java factory files
have a few strict requirements. This avoids the need to take care of every
corner case involving comments or strings. Since the generated factory file is
only used during the build, and the real one is used at runtime, it is
sufficient to have the build pass with simple modifications.

We require that these factory files to:
  - Contain exactly one top-level class.
    - No inner classes.
  - Contain a private constructor with no arguments (it won't be used).
  - Contain only public static methods apart from the constructor.
    - Only void and object return types are supported (no primitives).
  - Avoid explicitly importing internal classes and their dependencies.
    - The factory should be in the same package as the internal classes it
      instantiates.
"""

import argparse
import re
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__),
                             os.pardir, os.pardir, os.pardir,
                             'build', 'android', 'gyp'))
from util import build_utils


# Matches multi-line public static methods:
# '^(    public static ' - Matches the first part of the method signature.
# '(?!void)' - Do not match methods signatures that return void.
# '[^{]+ {)' - Matches the rest of the method signature until the opening brace.
# '.+?' - Non-greedy match to only match one method body.
# '^(    })$' - Matches the closing brace of the method.
_RE_PUBLIC_STATIC_NOT_VOID_METHOD = re.compile(
    r'^(    public static (?!void)[^{]+ {).+?^(    })$',
    re.DOTALL | re.MULTILINE)
# Replacement string to replace the entire method body with a null return.
_RETURN_NULL = r'''\g<1>
      return null;
\g<2>'''

# Matches multi-line public static void methods:
# Same as for public static methods, explicitly specify void return type.
_RE_PUBLIC_STATIC_VOID_METHOD = re.compile(
    r'^(    public static void [^{]+ {).+?^(    })$', re.DOTALL | re.MULTILINE)
# Replacement string to replace the entire method body with an empty return.
_RETURN = r'''\g<1>
      return;
\g<2>'''

_RE_CLASS = re.compile(r'\bclass (\w+)\b')
_PRIVATE_CONSTRUCTOR = 'private {0}() {{}}\n'


def _ParseArgs(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--input', required=True, help='Input java file path.')
  parser.add_argument('--output', required=True, help='Output java file path.')
  options = parser.parse_args(args)
  return options


def _EnsureSimpleFactory(content):
  """Assert that the java factory file is very simple.

  This is important since we are using the build system to swap out .class
  files of corresponding generated and internal java factory files. Keeping the
  factory files as simple as possible minimizes the overhead of reasoning about
  the factory code. It is already sufficiently complex to deduce which file a
  target is actually using when calling one of these factories.
  """
  classes = _RE_CLASS.findall(content)
  assert len(classes) == 1, 'Factory must contain exactly one class.'
  class_name = classes[0]
  assert _PRIVATE_CONSTRUCTOR.format(class_name) in content, (
      'Factory must have a private constructor with no arguments.')


def _ReplaceMethodBodies(content):
  content = _RE_PUBLIC_STATIC_NOT_VOID_METHOD.sub(_RETURN_NULL, content)
  content = _RE_PUBLIC_STATIC_VOID_METHOD.sub(_RETURN, content)
  return content


def main(args):
  options = _ParseArgs(args)
  with open(options.input, 'r') as f:
    content = f.read()
  _EnsureSimpleFactory(content)
  replaced_content = _ReplaceMethodBodies(content)
  with build_utils.AtomicOutput(options.output) as f:
    f.write(replaced_content)


if __name__ == '__main__':
  main(sys.argv[1:])
