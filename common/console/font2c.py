#!/usr/bin/python
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import sys


# This script converts from a human readable bitmap font format to a c source
# file containing the font as an array. It assumes a fixed width font.
#
# The input format:
# <char-width> <char-height>
# [REPEAT]{
# <character>
# *****
# *
# ***
# *
# *****
# }[REPEAT]
#
# The output will be an unsigned character array with the following format:
#
# unsigned char <symbol>[] = {
#   <char-width>, <char-height>,
#   [FOR i in 32 - 126]{
#    255, 255, 255, 255, 255, 0,
#    255, 0,   0,   0,   0,   0,
#    255, 255, 255, 0,   0,   0,
#    255, 0,   0,   0,   0,   0,
#    255, 255, 255, 255, 255, 0,
#   }[NEXT]


def main(argv):
  if len(argv) != 4:
    sys.stderr.write('USAGE: font2c.py <font-file> <symbol> <c-file>\n')
    return 1

  # Read in font file header.
  font_file = open(sys.argv[1], 'r')
  header_line = font_file.readline()
  header_parts = header_line.split(' ')
  width, height = [int(i) for i in header_parts]

  # Initialize everything to empty.
  font = dict()
  for ch in range(33, 127):
    font[ch] = [0] * width * height

  # Read each character.
  while True:
    line = font_file.readline()
    if len(line) < 2: break
    ch = ord(line[0])
    for y in range(height):
      row = font_file.readline()
      for x in range(min(width, len(row))):
        if row[x] == '*':
          font[ch][x + y * width] = 1
  font_file.close()

  # Emit array.
  c_file = open(sys.argv[3], 'w')
  c_file.write('unsigned char %s[] = {\n' % sys.argv[2])
  c_file.write('  /* %d x %d */\n')
  c_file.write('  %d, %d,\n' % (width, height))
  c_file.write('\n')
  for ch in range(33, 127):
    c_file.write('  /* [%s] */\n' % chr(ch))
    for y in range(height):
      c_file.write(' ')
      for x in range(width):
        c_file.write(' %d,' % (font[ch][x + y * width] * 255))
      c_file.write('\n')
    c_file.write('\n')

  c_file.write('};\n')
  c_file.close()

  return 0


sys.exit(main(sys.argv))

