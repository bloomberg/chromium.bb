# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate c++ structure mapping position to language code from .csv input."""

import argparse
import csv
import os.path
import string
import sys

sys.path.insert(1,
    os.path.join(os.path.dirname(__file__),
    os.path.pardir,
    os.path.pardir,
    "third_party"))
import jinja2 # pylint: disable=F0401


CELL_ID_FIELD = "cell_ids"
LANGUAGE_FIELD = "languages"


def ParseInputCsv(input_path):
    """Derive cell_id : language_code pairs."""
    district_with_languages = []
    with open(input_path, "r") as f:
        reader = csv.DictReader(f, delimiter=",")
        for row in reader:
            district_with_languages.append((row[LANGUAGE_FIELD],
                                            row[CELL_ID_FIELD]))
    return district_with_languages


def ExtractCellIds(language_districts):
    """Convert <language code>: [<cell_ids>...] into
       a list of cell_id : language_code pairs."""
    cell_language_code_pairs = []
    for language_code, cell_ids in language_districts:
        cell_ids = cell_ids.split(";")
        for cell_id in cell_ids:
            cell_language_code_pairs.append((cell_id, language_code))
    return cell_language_code_pairs


def GenerateCpp(output_path, template_path, cell_language_code_pairs):
    """Render the template to generate cpp code for LanguageCodeLocator."""
    with open(template_path, "r") as f:
        template = jinja2.Template(f.read())
        context = {
            "cell_lang_pairs" : cell_language_code_pairs
        }
        generated_code = template.render(context)

    # Write the generated code.
    with open(output_path, "w") as f:
        f.write(generated_code)


def Main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
      "--outputs", "-o", required=True,
      help="path to the generate c++ file")
    parser.add_argument(
      "--template", "-t", required=True,
      help="path to the template used to generate c++ file")
    parser.add_argument(
      "--inputs", "-i", required=True,
      help="path to the input .csv file")
    args = parser.parse_args()

    output_path = args.outputs
    template_file_path = args.template
    data_file_path = args.inputs

    cell_language_code_pairs = ExtractCellIds(ParseInputCsv(data_file_path))

    GenerateCpp(output_path, template_file_path, cell_language_code_pairs)

if __name__ == "__main__":
    Main()
