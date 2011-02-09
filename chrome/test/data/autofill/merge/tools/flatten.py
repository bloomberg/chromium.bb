# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

def main():
  """Converts a vertical serialization into a compact, horizontal serialization.

  """

  COLUMNS = ['First name', 'Middle name', 'Last name', 'Email', 'Company name',
             'Address line 1', 'Address line 2', 'City', 'State', 'Zipcode',
             'Country', 'Phone', 'Fax']

  if len(sys.argv) != 2:
    print "Usage: python flatten.py <path/to/serialized_profiles>"
    return

  profiles = [COLUMNS]
  with open(sys.argv[1], 'r') as serialized_profiles:
    profile = []
    for line in serialized_profiles:
      # Trim the newline if present.
      if line[-1] == '\n':
        line = line[:-1]

      if line == "---":
        if len(profile):
          # Reached the end of a profile.
          # Save the current profile and prepare to build up the next one.
          profiles.append(profile)
          profile = []
      else:
        # Append the current field's value to the current profile.
        field_value = line.split(': ', 1)[1]
        profile.append("'%s'" % field_value)

    if len(profile):
      profiles.append(profile)

  # Prepare format strings so that we can align the contents of each column.
  transposed = zip(*profiles)
  column_widths = []
  for column in transposed:
    widths = [len(value) for value in column]
    column_widths.append(max(widths))
  column_formats = ["{0:<" + str(width) + "}" for width in column_widths]

  for profile in profiles:
    profile_format = zip(column_formats, profile)
    profile = [format_.format(value) for (format_, value) in profile_format]
    print " | ".join(profile)


if __name__ == '__main__':
  main()