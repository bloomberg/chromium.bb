# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This module is not automatically loaded by the `cros` helper.  The filename
# would need a "cros_" prefix to make that happen.  It lives here so that it
# is alongside the cros_lint.py file.
#
# For msg namespaces, the 9xxx should generally be reserved for our own use.

"""Additional lint modules loaded by pylint.

This is loaded by pylint directly via its pylintrc file:
  load-plugins=chromite.cros.commands.lint

Then pylint will import the register function and call it.  So we can have
as many/few checkers as we want in this one module.
"""

from __future__ import print_function

import os
import sys

from pylint.checkers import BaseChecker
from pylint.interfaces import IASTNGChecker


class DocStringChecker(BaseChecker):
  """PyLint AST based checker to verify PEP 257 compliance

  See our style guide for more info:
  http://dev.chromium.org/chromium-os/python-style-guidelines#TOC-Describing-arguments-in-docstrings

  """
  # TODO: See about merging with the pep257 project:
  # https://github.com/GreenSteam/pep257

  __implements__ = IASTNGChecker

  name = 'doc_string_checker'
  priority = -1
  MSG_ARGS = 'offset:%(offset)i: {%(line)s}'
  msgs = {
      'C9001': ('Modules should have docstrings (even a one liner)',
                ('Used when a module lacks a docstring entirely')),
      'C9002': ('Classes should have docstrings (even a one liner)',
                ('Used when a class lacks a docstring entirely')),
      'C9003': ('Trailing whitespace in docstring'
                ': %s' % MSG_ARGS,
                ('Used whenever we find trailing whitespace')),
      'C9004': ('Leading whitespace in docstring (excess or missing)'
                ': %s' % MSG_ARGS,
                ('Used whenever we find incorrect leading whitespace')),
      'C9005': ('Closing triple quotes should not be cuddled',
                ('Used when the closing quotes are not by themselves')),
      'C9006': ('Section names should be preceded by one blank line'
                ': %s' % MSG_ARGS,
                ('Used when we detect misbehavior around sections')),
      'C9007': ('Section names should be "Args:", "Returns:", "Yields:", '
                'and "Raises:": %s' % MSG_ARGS,
                ('Used when we detect misbehavior around sections')),
      'C9008': ('Sections should be in the order: Args, Returns/Yields, Raises',
                ('Used when the various sections are misordered')),
      'C9009': ('First line should be a short summary',
                ('Used when a short doc string is on multiple lines')),
      'C9010': ('Not all args mentioned in doc string: |%(arg)s|',
                ('Used when not all arguments are in the doc string')),
      'C9011': ('Variable args/keywords are named *args/**kwargs, not %(arg)s',
                ('Used when funcs use different names for varargs')),
      'C9012': ('Incorrectly formatted Args section: %(arg)s',
                ('Used when spacing is incorrect after colon in Args')),
      'C9013': ('Too many blank lines in a row: %s' % MSG_ARGS,
                ('Used when more than one blank line is found')),
  }
  options = ()

  # TODO: Should we enforce Examples?
  VALID_SECTIONS = ('Args', 'Returns', 'Yields', 'Raises',)

  def visit_function(self, node):
    """Verify function docstrings"""
    if node.doc:
      lines = node.doc.split('\n')
      self._check_common(node, lines)
      self._check_last_line_function(node, lines)
      self._check_section_lines(node, lines)
      self._check_all_args_in_doc(node, lines)
      self._check_func_signature(node)
    else:
      # This is what C0111 already does for us, so ignore.
      pass

  def visit_module(self, node):
    """Verify module docstrings"""
    if node.doc:
      self._check_common(node)
    else:
      # Ignore stub __init__.py files.
      if os.path.basename(node.file) == '__init__.py':
        return
      self.add_message('C9001', node=node)

  def visit_class(self, node):
    """Verify class docstrings"""
    if node.doc:
      self._check_common(node)
    else:
      self.add_message('C9002', node=node, line=node.fromlineno)

  def _check_common(self, node, lines=None):
    """Common checks we enforce on all docstrings"""
    if lines is None:
      lines = node.doc.split('\n')

    funcs = (
        self._check_first_line,
        self._check_whitespace,
        self._check_last_line,
    )
    for f in funcs:
      f(node, lines)

  def _check_first_line(self, node, lines):
    """Make sure first line is a short summary by itself"""
    if lines[0] == '':
      self.add_message('C9009', node=node, line=node.fromlineno)

  def _check_whitespace(self, node, lines):
    """Verify whitespace is sane"""
    # Make sure first line doesn't have leading whitespace.
    if lines[0].lstrip() != lines[0]:
      margs = {'offset': 0, 'line': lines[0]}
      self.add_message('C9004', node=node, line=node.fromlineno, args=margs)

    # Verify no trailing whitespace.
    # We skip the last line since it's supposed to be pure whitespace.
    #
    # Also check for multiple blank lines in a row.
    last_blank = False
    for i, l in enumerate(lines[:-1]):
      margs = {'offset': i, 'line': l}

      if l.rstrip() != l:
        self.add_message('C9003', node=node, line=node.fromlineno, args=margs)

      curr_blank = l == ''
      if last_blank and curr_blank:
        self.add_message('C9013', node=node, line=node.fromlineno, args=margs)
      last_blank = curr_blank

    # Now specially handle the last line.
    l = lines[-1]
    if l.strip() != '' and l.rstrip() != l:
      margs = {'offset': len(lines), 'line': l}
      self.add_message('C9003', node=node, line=node.fromlineno, args=margs)

  def _check_last_line(self, node, lines):
    """Make sure last line is all by itself"""
    if len(lines) > 1:
      if lines[-1].strip() != '':
        self.add_message('C9005', node=node, line=node.fromlineno)

  def _check_last_line_function(self, node, lines):
    """Make sure last line is indented"""
    if len(lines) > 1:
      if lines[-1] == '':
        self.add_message('C9005', node=node, line=node.fromlineno)

  def _check_section_lines(self, node, lines):
    """Verify each section (Args/Returns/Yields/Raises) is sane"""
    lineno_sections = [-1] * len(self.VALID_SECTIONS)
    invalid_sections = (
        # Handle common misnamings.
        'arg', 'argument', 'arguments',
        'ret', 'rets', 'return',
        'yield', 'yeild', 'yeilds',
        'raise', 'throw', 'throws',
    )

    last = lines[0].strip()
    for i, line in enumerate(lines[1:]):
      margs = {'offset': i + 1, 'line': line}
      l = line.strip()

      # Catch semi-common javadoc style.
      if l.startswith('@param') or l.startswith('@return'):
        self.add_message('C9007', node=node, line=node.fromlineno, args=margs)

      # See if we can detect incorrect behavior.
      section = l.split(':', 1)[0]
      if section in self.VALID_SECTIONS or section.lower() in invalid_sections:
        # Make sure it has some number of leading whitespace.
        if not line.startswith(' '):
          self.add_message('C9004', node=node, line=node.fromlineno, args=margs)

        # Make sure it has a single trailing colon.
        if l != '%s:' % section:
          self.add_message('C9007', node=node, line=node.fromlineno, args=margs)

        # Make sure it's valid.
        if section.lower() in invalid_sections:
          self.add_message('C9007', node=node, line=node.fromlineno, args=margs)
        else:
          # Gather the order of the sections.
          lineno_sections[self.VALID_SECTIONS.index(section)] = i

        # Verify blank line before it.
        if last != '':
          self.add_message('C9006', node=node, line=node.fromlineno, args=margs)

      last = l

    # Make sure the sections are in the right order.
    valid_lineno = lambda x: x >= 0
    lineno_sections = filter(valid_lineno, lineno_sections)
    if lineno_sections != sorted(lineno_sections):
      self.add_message('C9008', node=node, line=node.fromlineno)

  def _check_all_args_in_doc(self, node, lines):
    """All function arguments are mentioned in doc"""
    if not hasattr(node, 'argnames'):
      return

    # Locate the start of the args section.
    arg_lines = []
    for l in lines:
      if arg_lines:
        if l.strip() in [''] + ['%s:' % x for x in self.VALID_SECTIONS]:
          break
      elif l.strip() != 'Args:':
        continue
      arg_lines.append(l)
    else:
      # If they don't have an Args section, then give it a pass.
      return

    # Now verify all args exist.
    # TODO: Should we verify arg order matches doc order ?
    # TODO: Should we check indentation of wrapped docs ?
    missing_args = []
    for arg in node.args.args:
      # Ignore class related args.
      if arg.name in ('cls', 'self'):
        continue
      # Ignore ignored args.
      if arg.name.startswith('_'):
        continue

      for l in arg_lines:
        aline = l.lstrip()
        if aline.startswith('%s:' % arg.name):
          amsg = aline[len(arg.name) + 1:]
          if len(amsg) and len(amsg) - len(amsg.lstrip()) != 1:
            margs = {'arg': l}
            self.add_message('C9012', node=node, line=node.fromlineno,
                             args=margs)
          break
      else:
        missing_args.append(arg.name)

    if missing_args:
      margs = {'arg': '|, |'.join(missing_args)}
      self.add_message('C9010', node=node, line=node.fromlineno, args=margs)

  def _check_func_signature(self, node):
    """Require *args to be named args, and **kwargs kwargs"""
    vararg = node.args.vararg
    if vararg and vararg != 'args' and vararg != '_args':
      margs = {'arg': vararg}
      self.add_message('C9011', node=node, line=node.fromlineno, args=margs)

    kwarg = node.args.kwarg
    if kwarg and kwarg != 'kwargs' and kwarg != '_kwargs':
      margs = {'arg': kwarg}
      self.add_message('C9011', node=node, line=node.fromlineno, args=margs)


class Py3kCompatChecker(BaseChecker):
  """Make sure we enforce py3k compatible features"""

  __implements__ = IASTNGChecker

  name = 'py3k_compat_checker'
  priority = -1
  MSG_ARGS = 'offset:%(offset)i: {%(line)s}'
  msgs = {
      'W9100': ('Missing "from __future__ import print_function" line',
                ('Used when a module misses print function import for py3k')),
  }
  options = ()

  def __init__(self, *args, **kwargs):
    super(Py3kCompatChecker, self).__init__(*args, **kwargs)
    self.seen_print_func = False
    self.saw_imports = False

  def close(self):
    """Called when done processing module"""
    if not self.seen_print_func:
      # Do not warn if moduler doesn't import anything at all (like
      # empty __init__.py files).
      if self.saw_imports:
        self.add_message('W9100')

  def _check_print_function(self, node):
    """Verify print_function is imported"""
    if node.modname == '__future__':
      for name, _ in node.names:
        if name == 'print_function':
          self.seen_print_func = True

  def visit_from(self, node):
    """Process 'from' statements"""
    self.saw_imports = True
    self._check_print_function(node)

  def visit_import(self, _node):
    """Process 'import' statements"""
    self.saw_imports = True


def register(linter):
  """pylint will call this func to register all our checkers"""
  # Walk all the classes in this module and register ours.
  this_module = sys.modules[__name__]
  for member in dir(this_module):
    if (not member.endswith('Checker') or
        member in ('BaseChecker', 'IASTNGChecker')):
      continue
    cls = getattr(this_module, member)
    linter.register_checker(cls(linter))
