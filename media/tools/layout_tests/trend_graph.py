#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fileinput
import os
import sys


"""A Module for manipulating trend graph with analyzer result history."""

DEFAULT_TREND_GRAPH_PATH = os.path.join('graph', 'graph.html')

# The following is necesasry to decide the point to insert.
LINE_INSERT_POINT_FOR_NUMBERS = r'// insert 1'
LINE_INSERT_POINT_FOR_PASSING_RATE = r'// insert 2'


class TrendGraph(object):
  """A class to manage trend graph which is using Google Visualization APIs.

  Google Visualization API (http://code.google.com/apis/chart/interactive/docs/
  gallery/annotatedtimeline.html) is used to present the historical analyzer
  result. Currently, data is directly written to JavaScript file using file
  in-place replacement for simplicity.

  TODO(imasaki): use GoogleSpreadsheet to store the analyzer result.
  """

  def __init__(self, location=DEFAULT_TREND_GRAPH_PATH):
    """Initialize this object with the location of trend graph."""
    self._location = location

  def Update(self, datetime_string, data_map):
    """Update trend graphs using |datetime_string| and |data_map|.

    There are two kinds of graphs to be updated (one is for numbers and the
    other is for passing rates).

    Args:
        datetime_string: a datetime string (e.g., '2008,1,1,13,45,00)'
        data_map: a dictionary containing 'whole', 'skip' , 'nonskip',
          'passingrate' as its keys and (number, tile, text) string tuples
          as values for graph annotation.
    """
    joined_str = ''
    for key in ['whole', 'skip', 'nonskip']:
      joined_str += ','.join(data_map[key]) + ','
    new_line_for_numbers = '         [new Date(%s),%s],\n' % (datetime_string,
                                                              joined_str)
    new_line_for_numbers += '         %s\n' % (
        LINE_INSERT_POINT_FOR_NUMBERS)
    self._ReplaceLine(LINE_INSERT_POINT_FOR_NUMBERS, new_line_for_numbers)

    joined_str = '%s,%s,%s' % (
        data_map['passingrate'][0], data_map['nonskip'][1],
        data_map['nonskip'][2])
    new_line_for_passingrate = '         [new Date(%s),%s],\n' % (
        datetime_string, joined_str)
    new_line_for_passingrate += '         %s\n' % (
        LINE_INSERT_POINT_FOR_PASSING_RATE)
    self._ReplaceLine(LINE_INSERT_POINT_FOR_PASSING_RATE,
                      new_line_for_passingrate)

  def _ReplaceLine(self, search_exp, replace_line):
    """Replace line which has |search_exp| with |replace_line|.

    Args:
        search_exp: search expression to find a line to be replaced.
        replace_line: the new line.
    """
    replaced = False
    for line in fileinput.input(self._location, inplace=1):
      if search_exp in line:
        replaced = True
        line = replace_line
      sys.stdout.write(line)
