# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from string import Template

import optparse
import os
import sys

try:
  grit_module_path = os.path.join(
      os.path.dirname(__file__),  '..', '..', 'tools', 'grit')
  sys.path.insert(0, grit_module_path)
  from grit.format import data_pack as DataPack
except ImportError, e:
  print 'ImportError: ', e
  sys.exit(-1)

header_template = \
"""// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_BLINK_RESOURCE_MAP_H_
#define COMPONENTS_HTML_VIEWER_BLINK_RESOURCE_MAP_H_

#include <map>

namespace html_viewer {

class BlinkResourceMap {
 public:
  BlinkResourceMap();
  const unsigned char* GetResource(int id, int* length);

 private:
  struct ResourceEntry {
    const unsigned char* data;
    int length;

    ResourceEntry()
      : data(nullptr)
      , length(0) {
    }

    ResourceEntry(const unsigned char* data, int length)
      : data(data)
      , length(length) {
    }
  };
  typedef std::map<int, ResourceEntry> ResourceMap;
  ResourceMap resources_;
};

} // namespace html_viewer
#endif // COMPONENTS_HTML_VIEWER_BLINK_RESOURCE_MAP_H_"""

cpp_template = \
"""// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "$header_file_name"

#include "base/macros.h"

namespace html_viewer {

$definitions

BlinkResourceMap::BlinkResourceMap()
{
  $map_initializer
}

const unsigned char* BlinkResourceMap::GetResource(int id, int* length)
{
  ResourceMap::iterator it = resources_.find(id);
  if (it == resources_.end()) {
    *length = 0;
    return nullptr;
  }
  *length = it->second.length;
  return it->second.data;
}

} // namespace html_viewer"""

def main():
  parser = optparse.OptionParser(
    usage='Usage: %prog --pak-file PAK_FILE --header HEADER --cpp CPP\n')
  parser.add_option('-i', '--pak-file', action='store', dest='pak_file',
                    help='The .pak file to be extracted.')
  parser.add_option('', '--header', action='store', dest='header_file',
                    help='Header file to be generated.')
  parser.add_option('', '--cpp', action='store', dest='cpp_file',
                    help='C++ file to be generated.')

  (options, _) = parser.parse_args()
  if (not options.pak_file or not options.header_file or not options.cpp_file):
    parser.print_help()
    sys.exit(-1)

  header_file = open(options.header_file, 'w+')
  cpp_file = open(options.cpp_file, 'w+')

  pak_contents = DataPack.ReadDataPack(options.pak_file)
  resourceIds = []

  header_contents = dict()
  cpp_contents = dict()

  definitions = []

  for (resId, data) in pak_contents.resources.iteritems():
    resourceIds.append(resId)
    hex_values = ['0x{0:02x}'.format(ord(char)) for char in data]
    f = lambda A, n=12: [A[i:i+n] for i in range(0, len(A), n)]
    hex_values_string = ',\n    '.join(', '.join(x) for x in f(hex_values))
    cpp_definition = \
        'const unsigned char kResource%s[%d] = {\n    %s \n};' % \
        (str(resId), len(hex_values), hex_values_string)
    definitions.append(cpp_definition)

  header_file_contents = Template(header_template).substitute(header_contents)
  header_file.write(header_file_contents)
  header_file.close()

  map_initializer = []
  for resId in resourceIds:
    insert_statement = \
        'resources_.insert(std::pair<int, ResourceEntry>(\n' \
        '      %s, ResourceEntry(kResource%s, arraysize(kResource%s))));'
    map_initializer.append( \
        insert_statement % (str(resId), str(resId), str(resId)))

  cpp_contents['definitions']= '\n'.join(definitions)
  cpp_contents['header_file_name'] = os.path.basename(options.header_file)
  cpp_contents['map_initializer'] = '\n  '.join(map_initializer)
  cpp_file_contents = Template(cpp_template).substitute(cpp_contents)
  cpp_file.write(cpp_file_contents)
  cpp_file.close()

if __name__ == '__main__':
  main()
