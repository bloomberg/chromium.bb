#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''python %prog [options] platform chromium_os_flag template

platform specifies which platform source is being generated for
  and can be one of (win, mac, linux)
chromium_os_flag should be 1 if this is a Chromium OS build
template is the path to a .json policy template file.'''

from __future__ import with_statement
from optparse import OptionParser
import re
import sys
import textwrap


CHROME_MANDATORY_SUBKEY = 'SOFTWARE\\\\Policies\\\\Google\\\\Chrome'
CHROME_RECOMMENDED_SUBKEY = CHROME_MANDATORY_SUBKEY + '\\\\Recommended'
CHROMIUM_MANDATORY_SUBKEY = 'SOFTWARE\\\\Policies\\\\Chromium'
CHROMIUM_RECOMMENDED_SUBKEY = CHROMIUM_MANDATORY_SUBKEY + '\\\\Recommended'


class PolicyDetails:
  """Parses a policy template and caches all its details."""

  # Maps policy types to a tuple with 3 other types:
  # - the equivalent base::Value::Type
  # - the equivalent Protobuf field type
  # - the name of one of the protobufs for shared policy types
  # TODO(joaodasilva): introduce a message to represent dictionary values.
  # Mapping 'dict' to 'string' for now. http://crbug.com/108997
  TYPE_MAP = {
    'dict':         ('TYPE_DICTIONARY',   'string',       'String'),
    'int':          ('TYPE_INTEGER',      'int64',        'Integer'),
    'int-enum':     ('TYPE_INTEGER',      'int64',        'Integer'),
    'list':         ('TYPE_LIST',         'StringList',   'StringList'),
    'main':         ('TYPE_BOOLEAN',      'bool',         'Boolean'),
    'string':       ('TYPE_STRING',       'string',       'String'),
    'string-enum':  ('TYPE_STRING',       'string',       'String'),
  }

  def __init__(self, policy, os, is_chromium_os):
    self.id = policy['id']
    self.name = policy['name']
    self.is_deprecated = policy.get('deprecated', False)
    self.is_device_only = policy.get('device_only', False)

    if is_chromium_os:
      expected_platform = 'chrome_os'
      wildcard_platform = None
    else:
      expected_platform = 'chrome.' + os.lower()
      wildcard_platform = 'chrome.*'
    is_supported = False
    for platform, version in [ p.split(':') for p in policy['supported_on'] ]:
      if (platform == expected_platform or platform == wildcard_platform) and \
          version.endswith('-'):
        is_supported = True
    self.is_supported = is_supported

    if not PolicyDetails.TYPE_MAP.has_key(policy['type']):
      raise NotImplementedError('Unknown policy type for %s: %s' %
                                (policy['name'], policy['type']))
    self.value_type, self.protobuf_type, self.policy_protobuf_type = \
        PolicyDetails.TYPE_MAP[policy['type']]

    self.desc = '\n'.join(
        map(str.strip, self._RemovePlaceholders(policy['desc']).splitlines()))
    self.caption = self._RemovePlaceholders(policy['caption'])

  PH_PATTERN = re.compile('<ph[^>]*>([^<]*|[^<]*<ex>([^<]*)</ex>[^<]*)</ph>')

  # Simplistic grit placeholder stripper.
  def _RemovePlaceholders(self, text):
    result = ''
    pos = 0
    for m in PolicyDetails.PH_PATTERN.finditer(text):
      result += text[pos:m.start(0)]
      result += m.group(2) or m.group(1)
      pos = m.end(0)
    result += text[pos:]
    return result


def main():
  parser = OptionParser(usage=__doc__)
  parser.add_option('--pch', '--policy-constants-header', dest='header_path',
                    help='generate header file of policy constants',
                    metavar='FILE')
  parser.add_option('--pcc', '--policy-constants-source', dest='source_path',
                    help='generate source file of policy constants',
                    metavar='FILE')
  parser.add_option('--cpp', '--cloud-policy-protobuf',
                    dest='cloud_policy_proto_path',
                    help='generate cloud policy protobuf file',
                    metavar='FILE')
  parser.add_option('--csp', '--chrome-settings-protobuf',
                    dest='chrome_settings_proto_path',
                    help='generate chrome settings protobuf file',
                    metavar='FILE')
  parser.add_option('--cpd', '--cloud-policy-decoder',
                    dest='cloud_policy_decoder_path',
                    help='generate C++ code decoding the cloud policy protobuf',
                    metavar='FILE')

  (opts, args) = parser.parse_args()

  if len(args) != 3:
    print 'exactly platform, chromium_os flag and input file must be specified.'
    parser.print_help()
    return 2

  os = args[0]
  is_chromium_os = args[1] == '1'
  template_file_name = args[2]

  template_file_contents = _LoadJSONFile(template_file_name)
  policy_details = [ PolicyDetails(policy, os, is_chromium_os)
                     for policy in _Flatten(template_file_contents) ]
  sorted_policy_details = sorted(policy_details, key=lambda policy: policy.name)

  def GenerateFile(path, writer, sorted=False):
    if path:
      with open(path, 'w') as f:
        _OutputGeneratedWarningHeader(f, template_file_name)
        writer(sorted and sorted_policy_details or policy_details, os, f)

  GenerateFile(opts.header_path, _WritePolicyConstantHeader, sorted=True)
  GenerateFile(opts.source_path, _WritePolicyConstantSource, sorted=True)
  GenerateFile(opts.cloud_policy_proto_path, _WriteCloudPolicyProtobuf)
  GenerateFile(opts.chrome_settings_proto_path, _WriteChromeSettingsProtobuf)
  GenerateFile(opts.cloud_policy_decoder_path, _WriteCloudPolicyDecoder)

  return 0


#------------------ shared helpers ---------------------------------#

def _OutputGeneratedWarningHeader(f, template_file_path):
  f.write('//\n'
          '// DO NOT MODIFY THIS FILE DIRECTLY!\n'
          '// IT IS GENERATED BY generate_policy_source.py\n'
          '// FROM ' + template_file_path + '\n'
          '//\n\n')


COMMENT_WRAPPER = textwrap.TextWrapper()
COMMENT_WRAPPER.width = 80
COMMENT_WRAPPER.initial_indent = '// '
COMMENT_WRAPPER.subsequent_indent = '// '
COMMENT_WRAPPER.replace_whitespace = False


# Writes a comment, each line prefixed by // and wrapped to 80 spaces.
def _OutputComment(f, comment):
  for line in comment.splitlines():
    if len(line) == 0:
      f.write('//')
    else:
      f.write(COMMENT_WRAPPER.fill(line))
    f.write('\n')


# Returns an iterator over all the policies in |template_file_contents|.
def _Flatten(template_file_contents):
  for policy in template_file_contents['policy_definitions']:
    if policy['type'] == 'group':
      for sub_policy in policy['policies']:
        yield sub_policy
    else:
      yield policy


def _LoadJSONFile(json_file):
  with open(json_file, 'r') as f:
    text = f.read()
  return eval(text)


#------------------ policy constants header ------------------------#

def _WritePolicyConstantHeader(policies, os, f):
  f.write('#ifndef CHROME_COMMON_POLICY_CONSTANTS_H_\n'
          '#define CHROME_COMMON_POLICY_CONSTANTS_H_\n'
          '\n'
          '#include <string>\n'
          '\n'
          '#include "base/values.h"\n'
          '\n'
          'namespace policy {\n\n')

  if os == 'win':
    f.write('// The windows registry path where mandatory policy '
            'configuration resides.\n'
            'extern const wchar_t kRegistryMandatorySubKey[];\n'
            '// The windows registry path where recommended policy '
            'configuration resides.\n'
            'extern const wchar_t kRegistryRecommendedSubKey[];\n\n')

  f.write('// Lists policy types mapped to their names and expected types.\n'
          '// Used to initialize ConfigurationPolicyProviders.\n'
          'struct PolicyDefinitionList {\n'
          '  struct Entry {\n'
          '    const char* name;\n'
          '    base::Value::Type value_type;\n'
          '    bool device_policy;\n'
          '    int id;\n'
          '  };\n'
          '\n'
          '  const Entry* begin;\n'
          '  const Entry* end;\n'
          '};\n'
          '\n'
          '// Returns true if the given policy is deprecated.\n'
          'bool IsDeprecatedPolicy(const std::string& policy);\n'
          '\n'
          '// Returns the default policy definition list for Chrome.\n'
          'const PolicyDefinitionList* GetChromePolicyDefinitionList();\n\n')
  f.write('// Key names for the policy settings.\n'
          'namespace key {\n\n')
  for policy in policies:
    # TODO(joaodasilva): Include only supported policies in
    # configuration_policy_handler.cc and configuration_policy_handler_list.cc
    # so that these names can be conditional on 'policy.is_supported'.
    # http://crbug.com/223616
    f.write('extern const char k' + policy.name + '[];\n')
  f.write('\n}  // namespace key\n\n'
          '}  // namespace policy\n\n'
          '#endif  // CHROME_COMMON_POLICY_CONSTANTS_H_\n')


#------------------ policy constants source ------------------------#

def _WritePolicyConstantSource(policies, os, f):
  f.write('#include "base/basictypes.h"\n'
          '#include "base/logging.h"\n'
          '#include "policy/policy_constants.h"\n'
          '\n'
          'namespace policy {\n\n')

  f.write('namespace {\n\n')

  f.write('const PolicyDefinitionList::Entry kEntries[] = {\n')
  for policy in policies:
    if policy.is_supported:
      f.write('  { key::k%s, Value::%s, %s, %s },\n' %
          (policy.name, policy.value_type,
              'true' if policy.is_device_only else 'false', policy.id))
  f.write('};\n\n')

  f.write('const PolicyDefinitionList kChromePolicyList = {\n'
          '  kEntries,\n'
          '  kEntries + arraysize(kEntries),\n'
          '};\n\n')

  f.write('// List of deprecated policies.\n'
          'const char* kDeprecatedPolicyList[] = {\n')
  for policy in policies:
    if policy.is_supported and policy.is_deprecated:
      f.write('  key::k%s,\n' % policy.name)
  f.write('};\n\n')

  f.write('}  // namespace\n\n')

  if os == 'win':
    f.write('#if defined(GOOGLE_CHROME_BUILD)\n'
            'const wchar_t kRegistryMandatorySubKey[] = '
            'L"' + CHROME_MANDATORY_SUBKEY + '";\n'
            'const wchar_t kRegistryRecommendedSubKey[] = '
            'L"' + CHROME_RECOMMENDED_SUBKEY + '";\n'
            '#else\n'
            'const wchar_t kRegistryMandatorySubKey[] = '
            'L"' + CHROMIUM_MANDATORY_SUBKEY + '";\n'
            'const wchar_t kRegistryRecommendedSubKey[] = '
            'L"' + CHROMIUM_RECOMMENDED_SUBKEY + '";\n'
            '#endif\n\n')

  f.write('bool IsDeprecatedPolicy(const std::string& policy) {\n'
          '  for (size_t i = 0; i < arraysize(kDeprecatedPolicyList);'
            ' ++i) {\n'
          '    if (policy == kDeprecatedPolicyList[i])\n'
          '      return true;\n'
          '  }\n'
          '  return false;\n'
          '}\n'
          '\n'
          'const PolicyDefinitionList* GetChromePolicyDefinitionList() {\n'
          '  return &kChromePolicyList;\n'
          '}\n\n')

  f.write('namespace key {\n\n')
  for policy in policies:
    # TODO(joaodasilva): Include only supported policies in
    # configuration_policy_handler.cc and configuration_policy_handler_list.cc
    # so that these names can be conditional on 'policy.is_supported'.
    # http://crbug.com/223616
    f.write('const char k{name}[] = "{name}";\n'.format(name=policy.name))
  f.write('\n}  // namespace key\n\n'
          '}  // namespace policy\n')


#------------------ policy protobufs --------------------------------#

CHROME_SETTINGS_PROTO_HEAD = '''
syntax = "proto2";

option optimize_for = LITE_RUNTIME;

package enterprise_management;

// For StringList and PolicyOptions.
import "cloud_policy.proto";

'''


CLOUD_POLICY_PROTO_HEAD = '''
syntax = "proto2";

option optimize_for = LITE_RUNTIME;

package enterprise_management;

message StringList {
  repeated string entries = 1;
}

message PolicyOptions {
  enum PolicyMode {
    // The given settings are applied regardless of user choice.
    MANDATORY = 0;
    // The user may choose to override the given settings.
    RECOMMENDED = 1;
    // No policy value is present and the policy should be ignored.
    UNSET = 2;
  }
  optional PolicyMode mode = 1 [default = MANDATORY];
}

message BooleanPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional bool value = 2;
}

message IntegerPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional int64 value = 2;
}

message StringPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional string value = 2;
}

message StringListPolicyProto {
  optional PolicyOptions policy_options = 1;
  optional StringList value = 2;
}

'''


# Field IDs [1..RESERVED_IDS] will not be used in the wrapping protobuf.
RESERVED_IDS = 2


def _WritePolicyProto(f, policy, fields):
  _OutputComment(f, policy.caption + '\n\n' + policy.desc)
  f.write('message %sProto {\n' % policy.name)
  f.write('  optional PolicyOptions policy_options = 1;\n')
  f.write('  optional %s %s = 2;\n' % (policy.protobuf_type, policy.name))
  f.write('}\n\n')
  fields += [ '  optional %sProto %s = %s;\n' %
              (policy.name, policy.name, policy.id + RESERVED_IDS) ]


def _WriteChromeSettingsProtobuf(policies, os, f):
  f.write(CHROME_SETTINGS_PROTO_HEAD)

  fields = []
  f.write('// PBs for individual settings.\n\n')
  for policy in policies:
    # Note: this protobuf also gets the unsupported policies, since it's an
    # exaustive list of all the supported user policies on any platform.
    if not policy.is_device_only:
      _WritePolicyProto(f, policy, fields)

  f.write('// --------------------------------------------------\n'
          '// Big wrapper PB containing the above groups.\n\n'
          'message ChromeSettingsProto {\n')
  f.write(''.join(fields))
  f.write('}\n\n')


def _WriteCloudPolicyProtobuf(policies, os, f):
  f.write(CLOUD_POLICY_PROTO_HEAD)
  f.write('message CloudPolicySettings {\n')
  for policy in policies:
    if policy.is_supported and not policy.is_device_only:
      f.write('  optional %sPolicyProto %s = %s;\n' %
              (policy.policy_protobuf_type, policy.name,
               policy.id + RESERVED_IDS))
  f.write('}\n\n')


#------------------ protobuf decoder -------------------------------#

CPP_HEAD = '''
#include <limits>
#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"
#include "policy/proto/cloud_policy.pb.h"

using google::protobuf::RepeatedPtrField;

namespace policy {

namespace em = enterprise_management;

Value* DecodeIntegerValue(google::protobuf::int64 value) {
  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    LOG(WARNING) << "Integer value " << value
                 << " out of numeric limits, ignoring.";
    return NULL;
  }

  return Value::CreateIntegerValue(static_cast<int>(value));
}

ListValue* DecodeStringList(const em::StringList& string_list) {
  ListValue* list_value = new ListValue;
  RepeatedPtrField<std::string>::const_iterator entry;
  for (entry = string_list.entries().begin();
       entry != string_list.entries().end(); ++entry) {
    list_value->Append(Value::CreateStringValue(*entry));
  }
  return list_value;
}

void DecodePolicy(const em::CloudPolicySettings& policy, PolicyMap* map) {
'''


CPP_FOOT = '''}

}  // namespace policy
'''


def _CreateValue(type, arg):
  if type == 'TYPE_BOOLEAN':
    return 'Value::CreateBooleanValue(%s)' % arg
  elif type == 'TYPE_INTEGER':
    return 'DecodeIntegerValue(%s)' % arg
  elif type == 'TYPE_STRING':
    return 'Value::CreateStringValue(%s)' % arg
  elif type == 'TYPE_LIST':
    return 'DecodeStringList(%s)' % arg
  elif type == 'TYPE_DICTIONARY':
    # TODO(joaodasilva): decode 'dict' types. http://crbug.com/108997
    return 'new DictionaryValue()'
  else:
    raise NotImplementedError('Unknown type %s' % type)


def _WritePolicyCode(f, policy):
  membername = policy.name.lower()
  proto_type = '%sPolicyProto' % policy.policy_protobuf_type
  f.write('  if (policy.has_%s()) {\n' % membername)
  f.write('    const em::%s& policy_proto = policy.%s();\n' %
          (proto_type, membername))
  f.write('    if (policy_proto.has_value()) {\n')
  f.write('      PolicyLevel level = POLICY_LEVEL_MANDATORY;\n'
          '      bool do_set = true;\n'
          '      if (policy_proto.has_policy_options()) {\n'
          '        do_set = false;\n'
          '        switch(policy_proto.policy_options().mode()) {\n'
          '          case em::PolicyOptions::MANDATORY:\n'
          '            do_set = true;\n'
          '            level = POLICY_LEVEL_MANDATORY;\n'
          '            break;\n'
          '          case em::PolicyOptions::RECOMMENDED:\n'
          '            do_set = true;\n'
          '            level = POLICY_LEVEL_RECOMMENDED;\n'
          '            break;\n'
          '          case em::PolicyOptions::UNSET:\n'
          '            break;\n'
          '        }\n'
          '      }\n'
          '      if (do_set) {\n')
  f.write('        Value* value = %s;\n' %
          (_CreateValue(policy.value_type, 'policy_proto.value()')))
  f.write('        map->Set(key::k%s, level, POLICY_SCOPE_USER, value);\n' %
          policy.name)
  f.write('      }\n'
          '    }\n'
          '  }\n')


def _WriteCloudPolicyDecoder(policies, os, f):
  f.write(CPP_HEAD)
  for policy in policies:
    if policy.is_supported and not policy.is_device_only:
      _WritePolicyCode(f, policy)
  f.write(CPP_FOOT)


if __name__ == '__main__':
  sys.exit(main())
