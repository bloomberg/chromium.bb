#!/usr/bin/python
# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module containing a function for presenting a text menu to the user."""

import sys


def _CeilDiv(numerator, denominator):
  """Do integer division, rounding up.

  >>> _CeilDiv(-1, 2)
  0
  >>> _CeilDiv(0, 2)
  0
  >>> _CeilDiv(1, 2)
  1
  >>> _CeilDiv(2, 2)
  1
  >>> _CeilDiv(3, 2)
  2

  Args:
    numerator: The number to divide.
    denominator: The number to divide by.

  Returns:
    (numberator) / denominator, rounded up.
  """
  return (numerator + denominator - 1) // denominator


def _BuildMenuStr(items, title, prompt, menu_width, spacing, add_quit):
  """Build the menu string for TextMenu.

  See TextMenu for a description.  This function mostly exists to simplify
  testing.

  >>> _BuildMenuStr(['A'], "Choose", "Choice", 76, 2, True)
  'Choose:\\n\\n1. A\\n\\nChoice (q to quit): '

  >>> _BuildMenuStr(['B', 'A'], "Choose", "Choice", 76, 2, False)
  'Choose:\\n\\n1. B  2. A\\n\\nChoice: '

  >>> _BuildMenuStr(['A', 'B', 'C', 'D', 'E'], "Choose", "Choice", 10, 2, False)
  'Choose:\\n\\n1. A  4. D\\n2. B  5. E\\n3. C\\n\\nChoice: '

  >>> _BuildMenuStr(['A', 'B', 'C', 'D', 'E'], "Choose", "Choice", 11, 2, False)
  'Choose:\\n\\n1. A  4. D\\n2. B  5. E\\n3. C\\n\\nChoice: '

  >>> _BuildMenuStr(['A', 'B', 'C'], "Choose", "Choice", 9, 2, False)
  'Choose:\\n\\n1. A\\n2. B\\n3. C\\n\\nChoice: '

  >>> _BuildMenuStr(['A'*10, 'B'*10], "Choose", "Choice", 0, 2, False)
  'Choose:\\n\\n1. AAAAAAAAAA\\n2. BBBBBBBBBB\\n\\nChoice: '

  >>> _BuildMenuStr([], "Choose", "Choice", 76, 2, False)
  Traceback (most recent call last):
  ...
  ValueError: Can't build a menu of empty choices.

  Args:
    items: See TextMenu().
    title: See TextMenu().
    prompt: See TextMenu().
    menu_width: See TextMenu().
    spacing: See TextMenu().
    add_quit: See TextMenu().

  Returns:
    See TextMenu().

  Raises:
    ValueError: If no items.
  """
  if not items:
    raise ValueError("Can't build a menu of empty choices.")

  # Figure out some basic stats about the items.
  num_items = len(items)
  longest_num = len(str(num_items))
  longest_item = longest_num + len(". ") + max(len(item) for item in items)

  # Figure out number of rows / cols.
  num_cols = max(1, (menu_width + spacing) // (longest_item + spacing))
  num_rows = _CeilDiv(num_items, num_cols)

  # Construct "2D array" of lines.  Remember that we go down first, then
  # right.  This seems to mimic "ls" behavior.  Note that, unlike "ls", we
  # currently make all columns have the same width.  Shrinking small columns
  # would be a nice optimization, but complicates the algorithm a bit.
  lines = [[] for _ in xrange(num_rows)]
  for item_num, item in enumerate(items):
    row = item_num % num_rows
    item_str = "%*d. %s" % (longest_num, item_num + 1, item)
    lines[row].append("%-*s" % (longest_item, item_str))

  # Change lines from 2D array into 1D array (1 entry per row) by joining
  # columns with spaces.
  spaces = " " * spacing
  lines = [spaces.join(line) for line in lines]

  # Add '(q to quit)' string to prompt if requested...
  if add_quit:
    prompt = "%s (q to quit)" % prompt

  # Make the final menu string by adding some return and the prompt.
  menu_str = "%s:\n\n%s\n\n%s: " % (title, "\n".join(lines), prompt)
  return menu_str


def TextMenu(items, title="Choose one", prompt="Choice",
             menu_width=76, spacing=4, add_quit=True):
  """Display text-based menu to the user and get back a response.

  The menu will be printed to sys.stderr and input will be read from sys.stdin.

  If the user doesn't want to choose something, he/she can use the 'q' to quit
  or press Ctrl-C (which will be caught and treated the same).

  The menu will look something like this:
    1. __init__.py   3. chromite      5. lib           7. tests
    2. bin           4. chroot_specs  6. specs

    Choice (q to quit):

  Args:
    items: The strings to show in the menu.  These should be sorted in whatever
        order you want to show to the user.
    title: The title of the menu.
    prompt: The prompt to show to the user.
    menu_width: The maximum width to use for the menu; 0 forces things to single
        column.
    spacing: The spacing between items.
    add_quit: Let the user type 'q' to quit the menu (we'll return None).

  Returns:
    The index of the item chosen by the user.  Note that this is a 0-based
    index, even though the user is presented with the menu in 1-based format.
    Will be None if the user hits Ctrl-C, or chooses q to quit.

  Raises:
    ValueError: If no items.
  """
  # Call the helper to build the actual menu string.
  menu_str = _BuildMenuStr(items, title, prompt, menu_width, spacing,
                           add_quit)

  # Loop until we get a valid input from the user (or they hit Ctrl-C, which
  # will throw and exception).
  while True:
    # Write the menu to stderr, which makes it possible to use this with
    # commands where you want the output redirected.
    sys.stderr.write(menu_str)

    # Don't use a prompt with raw_input(), since that would go to stdout.
    try:
      result = raw_input()
    except KeyboardInterrupt:
      # Consider this a quit.
      return None

    # Check for quit request
    if add_quit and result.lower() in ("q", "quit"):
      return None

    # Parse into a number and do error checking.  If all good, return.
    try:
      result_int = int(result)
      if 1 <= result_int <= len(items):
        # Convert from 1-based to 0-based index!
        return result_int - 1
      else:
        print >>sys.stderr, "\nERROR: %d out of range.\n\n" % result_int
    except ValueError:
      print >>sys.stderr, "\nERROR: '%s' is not a valid choice.\n\n" % result


def _Test():
  """Run any built-in tests."""
  import doctest
  doctest.testmod(verbose=True)


# For testing purposes, you can run this on the command line...
if __name__ == "__main__":
  # If first argument is --test, run testing code.
  # ...otherwise, pass all arguments as the menu to show.
  if sys.argv[1:2] == ["--test"]:
    _Test(*sys.argv[2:])
  else:
    if not sys.argv[1:]:
      print "ERROR: Need params to display as menu items"
    else:
      result = TextMenu(sys.argv[1:])
      print "You chose: '%s'" % result
