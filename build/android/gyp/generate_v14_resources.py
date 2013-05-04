#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Convert Android xml resources to API 14 compatible.

There are two reasons that we cannot just use API attributes,
so we are generating another set of resources by this script.

1. paddingStart attribute can cause a crash on Galaxy Tab 2.
2. There is a bug that paddingStart does not override paddingLeft on
   JB-MR1. This is fixed on JB-MR2.

Therefore, this resource generation script can be removed when
we drop the support for JB-MR1.

Please refer to http://crbug.com/235118 for the details.
"""

import optparse
import os
import re
import sys
import xml.dom.minidom as minidom

from util import build_utils


ATTRIBUTE_NAMESPACE = 'http://schemas.android.com/apk/res/android'

# Almost all the attributes that has "Start" or "End" in
# its name should be mapped.
ATTRIBUTES_TO_MAP = {'paddingStart' : 'paddingLeft',
                     'drawableStart' : 'drawableLeft',
                     'layout_alignStart' : 'layout_alignLeft',
                     'layout_marginStart' : 'layout_marginLeft',
                     'layout_alignParentStart' : 'layout_alignParentLeft',
                     'layout_toStartOf' : 'layout_toLeftOf',
                     'paddingEnd' : 'paddingRight',
                     'drawableEnd' : 'drawableRight',
                     'layout_alignEnd' : 'layout_alignRight',
                     'layout_marginEnd' : 'layout_marginRight',
                     'layout_alignParentEnd' : 'layout_alignParentRight',
                     'layout_toEndOf' : 'layout_toRightOf'}

ATTRIBUTES_TO_MAP_NS = {}

for k, v in ATTRIBUTES_TO_MAP.items():
  ATTRIBUTES_TO_MAP_NS[(ATTRIBUTE_NAMESPACE, k)] = (ATTRIBUTE_NAMESPACE, v)

ATTRIBUTES_TO_MAP_NS_VALUES = set(ATTRIBUTES_TO_MAP_NS.values())


def IterateXmlElements(node):
  """minidom helper function that iterates all the element nodes.
  Iteration order is pre-order depth-first."""
  if node.nodeType == node.ELEMENT_NODE:
    yield node
  for child_node in node.childNodes:
    for child_node_element in IterateXmlElements(child_node):
      yield child_node_element


def GenerateV14Resource(input_filename, output_filename):
  """Convert resource to API 14 compatible resource.

  It's mostly a simple replacement, s/Start/Left s/End/Right,
  on the attribute names.
  """
  dom = minidom.parse(input_filename)

  for element in IterateXmlElements(dom):
    all_names = element.attributes.keysNS()

    # Iterate all the attributes to find attributes to convert.
    # Note that name variable is actually a tuple that has namespace and name.
    # For example,
    # name == ('http://schemas.android.com/apk/res/android', 'paddingStart')
    for name, value in list(element.attributes.itemsNS()):
      # Note: gravity attributes are not necessary to convert because
      # start/end values are backward-compatible. Explained at
      # https://plus.sandbox.google.com/+RomanNurik/posts/huuJd8iVVXY?e=Showroom

      # Convert any other API 17 Start/End attributes to Left/Right attributes.
      # For example, from paddingStart="10dp" to paddingLeft="10dp"
      if name in ATTRIBUTES_TO_MAP_NS:
        mapped_name = ATTRIBUTES_TO_MAP_NS[name]

        # Add the new mapped attribute and remove the original attribute.
        # For example, add paddingLeft and remove paddingStart.
        # Note that instead of element.setAttribute(...), this is more correct.
        # element.setAttributeNS(mapped_name[0], mapped_name[1], value)
        # However, there is a minidom bug that doesn't print namespace set by
        # setAttributeNS. Hence this workaround.
        # This is a similar bug discussion about minidom namespace normalizing.
        # http://stackoverflow.com/questions/863774/how-to-generate-xml-documents-with-namespaces-in-python
        element.setAttribute('android:' + mapped_name[1], value)
        del element.attributes[name]
      elif name in ATTRIBUTES_TO_MAP_NS_VALUES:
        # TODO(kkimlabs): Enable  warning once layouts have been converted
        # print >> sys.stderror, 'Warning: layout should use xxx instead of yyy'
        pass

  build_utils.MakeDirectory(os.path.dirname(output_filename))
  with open(output_filename, 'w') as f:
    dom.writexml(f, '  ', '\n', encoding='utf-8')


def GenerateV14ResourcesInDir(input_dir, output_dir):
  """Convert resources to API 14 compatible XML resources in the directory."""
  for input_file in build_utils.FindInDirectory(input_dir, '*.xml'):
    output_path = os.path.join(output_dir,
                               os.path.relpath(input_file, input_dir))
    GenerateV14Resource(input_file, output_path)


def ParseArgs():
  """Parses command line options.

  Returns:
    An options object as from optparse.OptionsParser.parse_args()
  """
  parser = optparse.OptionParser()
  parser.add_option('--res-dir',
                    help='directory containing resources '
                         'used to generate v14 resources')
  parser.add_option('--res-v14-dir',
                    help='output directory into which '
                         'v14 resources will be generated')
  parser.add_option('--stamp', help='File to touch on success')

  options, args = parser.parse_args()

  if args:
    parser.error('No positional arguments should be given.')

  # Check that required options have been provided.
  required_options = ('res_dir', 'res_v14_dir')
  build_utils.CheckOptions(options, parser, required=required_options)
  return options


def main(argv):
  options = ParseArgs()

  build_utils.DeleteDirectory(options.res_v14_dir)
  build_utils.MakeDirectory(options.res_v14_dir)

  for name in os.listdir(options.res_dir):
    if not os.path.isdir(os.path.join(options.res_dir, name)):
      continue

    dir_pieces = name.split('-')
    resource_type = dir_pieces[0]
    qualifiers = dir_pieces[1:]

    # We only convert resources under layout*/ and xml*/.
    if resource_type not in ('layout', 'xml'):
      continue

    # Android pre-v17 API doesn't support RTL. Skip.
    if 'ldrtl' in qualifiers:
      continue

    # Convert all the resource files.
    input_path = os.path.join(options.res_dir, name)
    output_path = os.path.join(options.res_v14_dir, name)
    GenerateV14ResourcesInDir(input_path, output_path)

  if options.stamp:
    build_utils.Touch(options.stamp)

if __name__ == '__main__':
  sys.exit(main(sys.argv))

