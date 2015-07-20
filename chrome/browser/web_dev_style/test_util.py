# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def GetHighlight(line, error):
  """Returns the substring of |line| that is highlighted in |error|."""
  error_lines = error.split('\n')
  highlight = error_lines[error_lines.index(line) + 1]
  return ''.join(ch1 for (ch1, ch2) in zip(line, highlight) if ch2 == '^')
