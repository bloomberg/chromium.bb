# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate c++ structure containing serialized ULP language quad tree"""

import argparse
import csv
import os.path
import string
import sys
import array

sys.path.insert(1,
    os.path.join(os.path.dirname(__file__),
    os.path.pardir,
    os.path.pardir,
    os.path.pardir,
    os.path.pardir,
    os.path.pardir,
    "third_party"))
import jinja2 # pylint: disable=F0401


def ReadSerializedData(input_path):
  """Read serialized ULP language quad tree"""
  with open(input_path) as input_file:
    data = input_file.read()
  linebreak = data.index('\n')
  # First line is comma-separated list of languages.
  language_codes = data[:linebreak].strip().split(',')
  # Rest of the file is the serialized tree. We read the bits as 32 bits,
  # unsigned int words.
  tree_serialized = array.array('I', data[linebreak+1:])
  assert tree_serialized.itemsize == 4, "Items must be 4 bytes ints."
  return tree_serialized, language_codes


def GenerateCpp(output_path,
                template_path,
                tree_serialized,
                language_codes):
    """Render the template"""
    with open(template_path, "r") as f:
        template = jinja2.Template(f.read())
        context = {
            "tree_serialized" : tree_serialized,
            "languages" : language_codes,
        }
        generated_code = template.render(context)

    # Write the generated code.
    with open(output_path, "w") as f:
        f.write(generated_code)


def Main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
      "--output", required=True,
      help="path to the generated c++ file")
    parser.add_argument(
      "--template", required=True,
      help="path to the template used to generate c++ file")
    parser.add_argument(
      "--data", required=True,
      help="path to the input serialized ULP data file")
    args = parser.parse_args()

    output_path = args.output
    template_path = args.template
    data_path = args.data

    tree_serialized, language_codes = ReadSerializedData(data_path)

    GenerateCpp(output_path,
                template_path,
                tree_serialized,
                language_codes)

if __name__ == "__main__":
    Main()
