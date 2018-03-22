# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fileinput
import glob
import optparse
import os
import re
import shlex
import sys
import textwrap

CANVAS_DIMENSIONS = "CANVAS_DIMENSIONS"
CPP_COMMENT_DELIMITER = "//"
REFERENCE_SIZE_DIP = 48
TEMPLATE_PLACEHOLDER = "TEMPLATE_PLACEHOLDER"


def Error(msg):
  print >> sys.stderr, msg
  sys.exit(1)


def CamelCase(name, suffix):
  words = name.split("_")
  words = [w.capitalize() for w in words]
  return "k" + "".join(words) + suffix


def GetPathName(name, append_1x=False):
  return CamelCase(name, "Path1x" if append_1x else "Path")


def GetRepName(name, append_1x=False):
  return CamelCase(name, "Rep1x" if append_1x else "Rep")


def GetIconName(name):
  return CamelCase(name, "Icon")


def ExtractIconReps(icon_file_name):
  """Reads the contents of the given icon file and returns a list of vector
     commands for different icon representations stored in that file.

  Args:
      icon_file_name: The file path of the icon file to read.
  """
  with open(icon_file_name, "r") as icon_file:
    icon_file_contents = icon_file.readlines()

  icon_sizes = []
  current_icon_representation = []
  icon_representation_list = []
  for line in icon_file_contents:
    # Strip comments and empty lines.
    line = line.partition(CPP_COMMENT_DELIMITER)[0].strip()
    if not line:
      continue
    # Retrieve sizes specified by CANVAS_DIMENSIONS to ensure icons are added in
    # sorted order by size descending.
    if line.startswith(CANVAS_DIMENSIONS):
      sizes = re.findall(r"\d+", line)
      if len(sizes) != 1:
        Error("Malformed {} line found in {} - it should only specify one size."
              .format(CANVAS_DIMENSIONS, icon_file_name))
      icon_sizes.append(int(sizes[0]))

      # All icons except the first / default icon must start with
      # "CANVAS_DIMENSIONS", so rely on it here as a icon delimiter.
      if current_icon_representation:
        icon_representation_list.append("\n".join(current_icon_representation))
        current_icon_representation = []
    current_icon_representation.append(line)
  if current_icon_representation:
    icon_representation_list.append("\n".join(current_icon_representation))

  if not icon_representation_list:
    Error("Didn't find any icons in {}.".format(icon_file_name))

  if len(icon_representation_list) != len(icon_sizes):
    icon_sizes.insert(0, REFERENCE_SIZE_DIP)
  if sorted(icon_sizes, reverse=True) != icon_sizes:
    Error("The icons in {} should be sorted in descending order of size."
          .format(icon_file_name))
  return icon_representation_list


def AggregateVectorIcons(working_directory, file_list, output_cc, output_h):
  """Compiles all .icon files in a directory into two C++ files.

  Args:
      working_directory: The path to the directory that holds the .icon files
          and C++ templates.
      file_list: A file containing the list of vector icon files to process.
      output_cc: The path that should be used to write the .cc file.
      output_h: The path that should be used to write the .h file.
  """

  # For each file in |file_list|, place it in |path_map| if its extension is
  # .icon. This will map the icon's name to its path, e.g.,
  # path_map['cat'] = 'foo/bar/cat.icon'.
  icon_list = []
  with open(file_list, "r") as f:
    file_list_contents = f.read()
  icon_list = shlex.split(file_list_contents)

  path_map = {}

  for icon_path in icon_list:
    (icon_name, extension) = os.path.splitext(os.path.basename(icon_path))

    if extension != ".icon":
      Error("Only filenames " + icon_name + ".icon are allowed.")

    if icon_name not in path_map:
      path_map[icon_name] = icon_path
    else:
      Error("A vector icon with name '" + icon_name + "' already exists.")

  # Generate the file vector_icons.h which declares a variable for each
  # icon in |path_map|. The variable name is derived from the icon name by
  # converting to camel case, prepending 'k', and appending 'Icon'. For
  # example, the icon 'foo_bar' will have the variable name kFooBarIcon.
  with open(os.path.join(working_directory, "vector_icons.h.template"),
            "r") as input_header_template:
    header_template_contents = input_header_template.readlines()

  output_header = open(output_h, "w")
  for line in header_template_contents:
    if not TEMPLATE_PLACEHOLDER in line:
      output_header.write(line)
      continue

    for icon in path_map:
      (icon_name, extension) = os.path.splitext(
                               os.path.basename(path_map[icon]))
      output_header.write("VECTOR_ICON_TEMPLATE_H({})\n".format(
          GetIconName(icon_name)))
  output_header.close()

  # Copy the vector icon drawing commands from the .icon files, splitting them
  # into their individual icon representations, and use them to generate
  # vector_icons.cc, which defines the variables declared in vector_icons.h.
  input_cc_template = open(
      os.path.join(working_directory, "vector_icons.cc.template"))
  cc_template_contents = input_cc_template.readlines()
  input_cc_template.close()

  output_cc = open(output_cc, "w")
  for line in cc_template_contents:
    if not TEMPLATE_PLACEHOLDER in line:
      output_cc.write(line)
      continue
    for icon in path_map:
      (icon_name, extension) = os.path.splitext(
                               os.path.basename(path_map[icon]))

      # Store the vector-drawing commands for foo_bar.icon in the temporary
      # variable kFooBarPath.
      icon_representation_list = ExtractIconReps(path_map[icon])
      (vector_commands, vector_commands_1x) = None, None
      if len(icon_representation_list) >= 1:
        vector_commands = icon_representation_list[0]
      if len(icon_representation_list) >= 2:
        vector_commands_1x = icon_representation_list[1]
      if len(icon_representation_list) >= 3:
        print "Warning: " + path_map[icon] + " has more than two icon", \
              "representations. Support for this is still a WIP - see", \
              "crbug.com/647286. For now, only the first two will be used."
      output_cc.write("VECTOR_ICON_REP_TEMPLATE({}, {}, {})\n".format(
          GetPathName(icon_name), GetRepName(icon_name), vector_commands))

      # Store the vector-drawing commands for foo_bar.icon's 1x version in the
      # temporary variable kFooBarPath1x, if it exists.
      if vector_commands_1x:
        output_cc.write("VECTOR_ICON_REP_TEMPLATE({}, {}, {})\n".format(
            GetPathName(icon_name, True), GetRepName(icon_name, True),
            vector_commands_1x))
        output_cc.write("VECTOR_ICON_TEMPLATE2({}, {}, {})\n".format(
            GetIconName(icon_name), GetRepName(icon_name),
            GetRepName(icon_name, True)))
      else:
        # Define the value of kFooBarIcon.
        output_cc.write("VECTOR_ICON_TEMPLATE({}, {})\n".format(
            GetIconName(icon_name), GetRepName(icon_name)))
  output_cc.close()


def main():
  parser = optparse.OptionParser()
  parser.add_option("--working_directory",
                    help="The directory to look for template C++ as well as "
                         "icon files.")
  parser.add_option("--file_list",
                    help="A response file containing the list of icon files "
                         "to be processed.")
  parser.add_option("--output_cc",
                    help="The path to output the CC file to.")
  parser.add_option("--output_h",
                    help="The path to output the header file to.")

  (options, args) = parser.parse_args()

  AggregateVectorIcons(options.working_directory,
                       options.file_list,
                       options.output_cc,
                       options.output_h)


if __name__ == "__main__":
  main()
