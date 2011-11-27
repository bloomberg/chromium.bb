#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''
Checks a policy_templates.json file for conformity to its syntax specification.
'''

import json
import optparse
import os
import re
import sys


LEADING_WHITESPACE = re.compile('^([ \t]*)')
TRAILING_WHITESPACE = re.compile('.*?([ \t]+)$')
# Matches all non-empty strings that contain no whitespaces.
NO_WHITESPACE = re.compile('[^\s]+$')


class PolicyTemplateChecker(object):

  def __init__(self):
    self.error_count = 0
    self.warning_count = 0
    self.num_policies = 0
    self.num_groups = 0
    self.num_policies_in_groups = 0
    self.options = None

  def _Error(self, message, parent_element=None, identifier=None,
             offending_snippet=None):
    self.error_count += 1
    error = ''
    if identifier is not None and parent_element is not None:
      error += 'In %s %s: ' % (parent_element, identifier)
    print error + 'Error: ' + message
    if offending_snippet is not None:
      print '  Offending:', json.dumps(offending_snippet, indent=2)

  def _CheckContains(self, container, key, value_type,
                     optional=False,
                     parent_element='policy',
                     container_name=None,
                     identifier=None,
                     offending='__CONTAINER__',
                     regexp_check=None):
    '''
    Checks |container| for presence of |key| with value of type |value_type|.
    If |value_type| is string and |regexp_check| is specified, then an error is
    reported when the value does not match the regular expression object.

    The other parameters are needed to generate, if applicable, an appropriate
    human-readable error message of the following form:

    In |parent_element| |identifier|:
      (if the key is not present):
      Error: |container_name| must have a |value_type| named |key|.
      Offending snippet: |offending| (if specified; defaults to |container|)
      (if the value does not have the required type):
      Error: Value of |key| must be a |value_type|.
      Offending snippet: |container[key]|

    Returns: |container[key]| if the key is present, None otherwise.
    '''
    if identifier is None:
      identifier = container.get('name')
    if container_name is None:
      container_name = parent_element
    if offending == '__CONTAINER__':
      offending = container
    if key not in container:
      if optional:
        return
      else:
        self._Error('%s must have a %s "%s".' %
                    (container_name.title(), value_type.__name__, key),
                    container_name, identifier, offending)
      return None
    value = container[key]
    if not isinstance(value, value_type):
      self._Error('Value of "%s" must be a %s.' %
                  (key, value_type.__name__),
                  container_name, identifier, value)
    if value_type == str and regexp_check and not regexp_check.match(value):
      self._Error('Value of "%s" must match "%s".' %
                  (key, regexp_check.pattern),
                  container_name, identifier, value)
    return value

  def _AddPolicyID(self, id, policy_ids, policy):
    '''
    Adds |id| to |policy_ids|. Generates an error message if the
    |id| exists already; |policy| is needed for this message.
    '''
    if id in policy_ids:
      self._Error('Duplicate id', 'policy', policy.get('name'),
                  id)
    else:
      policy_ids.add(id)

  def _CheckPolicyIDs(self, policy_ids):
    '''
    Checks a set of policy_ids to make sure it contains a continuous range
    of entries (i.e. no holes).
    Holes would not be a technical problem, but we want to ensure that nobody
    accidentally omits IDs.
    '''
    for i in range(len(policy_ids)):
      if (i + 1) not in policy_ids:
        self._Error('No policy with id: %s' % (i + 1))

  def _CheckPolicy(self, policy, is_in_group, policy_ids):
    if not isinstance(policy, dict):
      self._Error('Each policy must be a dictionary.', 'policy', None, policy)
      return

    # There should not be any unknown keys in |policy|.
    for key in policy:
      if key not in ('name', 'type', 'caption', 'desc', 'device_only',
                     'supported_on', 'label', 'policies', 'items',
                     'example_value', 'features', 'deprecated', 'future',
                     'id'):
        self.warning_count += 1
        print ('In policy %s: Warning: Unknown key: %s' %
               (policy.get('name'), key))

    # Each policy must have a name.
    self._CheckContains(policy, 'name', str, regexp_check=NO_WHITESPACE)

    # Each policy must have a type.
    policy_type = self._CheckContains(policy, 'type', str)
    if policy_type not in ('group', 'main', 'string', 'int', 'list', 'int-enum',
                           'string-enum'):
      self._Error('Policy type must be either of: group, main, string, int, '
                  'list, int-enum, string-enum', 'policy', policy, policy_type)
      return  # Can't continue for unsupported type.

    # Each policy must have a caption message.
    self._CheckContains(policy, 'caption', str)

    # Each policy must have a description message.
    self._CheckContains(policy, 'desc', str)

    # If 'label' is present, it must be a string.
    self._CheckContains(policy, 'label', str, True)

    # If 'deprecated' is present, it must be a bool.
    self._CheckContains(policy, 'deprecated', bool, True)

    # If 'future' is present, it must be a bool.
    self._CheckContains(policy, 'future', bool, True)

    if policy_type == 'group':

      # Groups must not be nested.
      if is_in_group:
        self._Error('Policy groups must not be nested.', 'policy', policy)

      # Each policy group must have a list of policies.
      policies = self._CheckContains(policy, 'policies', list)

      # Check sub-policies.
      if policies is not None:
        for nested_policy in policies:
          self._CheckPolicy(nested_policy, True, policy_ids)

      # Groups must not have an |id|.
      if 'id' in policy:
        self._Error('Policies of type "group" must not have an "id" field.',
                    'policy', policy)

      # Statistics.
      self.num_groups += 1
    else:  # policy_type != group

      # Each policy must have a protobuf ID.
      id = self._CheckContains(policy, 'id', int)
      self._AddPolicyID(id, policy_ids, policy)

      # Each policy must have a supported_on list.
      supported_on = self._CheckContains(policy, 'supported_on', list)
      if supported_on is not None:
        for s in supported_on:
          if not isinstance(s, str):
            self._Error('Entries in "supported_on" must be strings.', 'policy',
                        policy, supported_on)

      # Each policy must have a 'features' dict.
      self._CheckContains(policy, 'features', dict)

      # Each policy must have an 'example_value' of appropriate type.
      if policy_type == 'main':
        value_type = bool
      elif policy_type in ('string', 'string-enum'):
        value_type = str
      elif policy_type in ('int', 'int-enum'):
        value_type = int
      elif policy_type == 'list':
        value_type = list
      else:
        raise NotImplementedError('Unimplemented policy type: %s' % policy_type)
      self._CheckContains(policy, 'example_value', value_type)

      # Statistics.
      self.num_policies += 1
      if is_in_group:
        self.num_policies_in_groups += 1

    if policy_type in ('int-enum', 'string-enum'):

      # Enums must contain a list of items.
      items = self._CheckContains(policy, 'items', list)
      if items is not None:
        if len(items) < 1:
          self._Error('"items" must not be empty.', 'policy', policy, items)
        for item in items:
          # Each item must have a name.
          # Note: |policy.get('name')| is used instead of |policy['name']|
          # because it returns None rather than failing when no key called
          # 'name' exists.
          self._CheckContains(item, 'name', str, container_name='item',
                              identifier=policy.get('name'),
                              regexp_check=NO_WHITESPACE)

          # Each item must have a value of the correct type.
          self._CheckContains(item, 'value', value_type, container_name='item',
                              identifier=policy.get('name'))

          # Each item must have a caption.
          self._CheckContains(item, 'caption', str, container_name='item',
                              identifier=policy.get('name'))

  def _CheckMessage(self, key, value):
    # |key| must be a string, |value| a dict.
    if not isinstance(key, str):
      self._Error('Each message key must be a string.', 'message', key, key)
      return

    if not isinstance(value, dict):
      self._Error('Each message must be a dictionary.', 'message', key, value)
      return

    # Each message must have a desc.
    self._CheckContains(value, 'desc', str, parent_element='message',
                        identifier=key)

    # Each message must have a text.
    self._CheckContains(value, 'text', str, parent_element='message',
                        identifier=key)

    # There should not be any unknown keys in |value|.
    for vkey in value:
      if vkey not in ('desc', 'text'):
        self.warning_count += 1
        print 'In message %s: Warning: Unknown key: %s' % (key, vkey)

  def _LeadingWhitespace(self, line):
    match = LEADING_WHITESPACE.match(line)
    if match:
      return match.group(1)
    return ''

  def _TrailingWhitespace(self, line):
    match = TRAILING_WHITESPACE.match(line)
    if match:
      return match.group(1)
    return ''

  def _LineError(self, message, line_number):
    self.error_count += 1
    print 'In line %d: Error: %s' % (line_number, message)

  def _LineWarning(self, message, line_number):
    self.warning_count += 1
    print ('In line %d: Warning: Automatically fixing formatting: %s'
           % (line_number, message))

  def _CheckFormat(self, filename):
    if self.options.fix:
      fixed_lines = []
    with open(filename) as f:
      indent = 0
      line_number = 0
      for line in f:
        line_number += 1
        line = line.rstrip('\n')
        # Check for trailing whitespace.
        trailing_whitespace = self._TrailingWhitespace(line)
        if len(trailing_whitespace) > 0:
          if self.options.fix:
            line = line.rstrip()
            self._LineWarning('Trailing whitespace.', line_number)
          else:
            self._LineError('Trailing whitespace.', line_number)
        if self.options.fix:
          if len(line) == 0:
            fixed_lines += ['\n']
            continue
        else:
          if line == trailing_whitespace:
            # This also catches the case of an empty line.
            continue
        # Check for correct amount of leading whitespace.
        leading_whitespace = self._LeadingWhitespace(line)
        if leading_whitespace.count('\t') > 0:
          if self.options.fix:
            leading_whitespace = leading_whitespace.replace('\t', '  ')
            line = leading_whitespace + line.lstrip()
            self._LineWarning('Tab character found.', line_number)
          else:
            self._LineError('Tab character found.', line_number)
        if line[len(leading_whitespace)] in (']', '}'):
          indent -= 2
        if line[0] != '#':  # Ignore 0-indented comments.
          if len(leading_whitespace) != indent:
            if self.options.fix:
              line = ' ' * indent + line.lstrip()
              self._LineWarning('Indentation should be ' + str(indent) +
                                ' spaces.', line_number)
            else:
              self._LineError('Bad indentation. Should be ' + str(indent) +
                              ' spaces.', line_number)
        if line[-1] in ('[', '{'):
          indent += 2
        if self.options.fix:
          fixed_lines.append(line + '\n')

    # If --fix is specified: backup the file (deleting any existing backup),
    # then write the fixed version with the old filename.
    if self.options.fix:
      if self.options.backup:
        backupfilename = filename + '.bak'
        if os.path.exists(backupfilename):
          os.remove(backupfilename)
        os.rename(filename, backupfilename)
      with open(filename, 'w') as f:
        f.writelines(fixed_lines)

  def Main(self, filename, options):
    try:
      with open(filename) as f:
        data = eval(f.read())
    except:
      import traceback
      traceback.print_exc(file=sys.stdout)
      self._Error('Invalid JSON syntax.')
      return
    if data == None:
      self._Error('Invalid JSON syntax.')
      return
    self.options = options

    # First part: check JSON structure.

    # Check policy definitions.
    policy_definitions = self._CheckContains(data, 'policy_definitions', list,
                                             parent_element=None,
                                             container_name='The root element',
                                             offending=None)
    if policy_definitions is not None:
      policy_ids = set()
      for policy in policy_definitions:
        self._CheckPolicy(policy, False, policy_ids)
      self._CheckPolicyIDs(policy_ids)

    # Check (non-policy-specific) message definitions.
    messages = self._CheckContains(data, 'messages', dict,
                                   parent_element=None,
                                   container_name='The root element',
                                   offending=None)
    if messages is not None:
      for message in messages:
        self._CheckMessage(message, messages[message])

    # Second part: check formatting.
    self._CheckFormat(filename)

    # Third part: summary and exit.
    print ('Finished checking %s. %d errors, %d warnings.' %
        (filename, self.error_count, self.warning_count))
    if self.options.stats:
      if self.num_groups > 0:
        print ('%d policies, %d of those in %d groups (containing on '
               'average %.1f policies).' %
               (self.num_policies, self.num_policies_in_groups, self.num_groups,
                 (1.0 * self.num_policies_in_groups / self.num_groups)))
      else:
        print self.num_policies, 'policies, 0 policy groups.'
    if self.error_count > 0:
      return 1
    return 0

  def Run(self, argv, filename=None):
    parser = optparse.OptionParser(
        usage='usage: %prog [options] filename',
        description='Syntax check a policy_templates.json file.')
    parser.add_option('--fix', action='store_true',
                      help='Automatically fix formatting.')
    parser.add_option('--backup', action='store_true',
                      help='Create backup of original file (before fixing).')
    parser.add_option('--stats', action='store_true',
                      help='Generate statistics.')
    (options, args) = parser.parse_args(argv)
    if filename is None:
      if len(args) != 2:
        parser.print_help()
        sys.exit(1)
      filename = args[1]
    return self.Main(filename, options)


if __name__ == '__main__':
  sys.exit(PolicyTemplateChecker().Run(sys.argv))
