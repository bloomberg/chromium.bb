#!/usr/bin/python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys
import subprocess


# Path to root FFmpeg directory.  Used as the CWD for executing all commands.
FFMPEG_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..'))

# Path to the C99 to C89 converter.
CONVERTER_EXECUTABLE = os.path.abspath(os.path.join(
    FFMPEG_ROOT, 'chromium', 'binaries', 'c99conv.exe'))

# $CC to use.   We use to support GOMA, but by switching to stdout preprocessing
# its twice as fast to just preprocess locally (compilation may still be GOMA).
DEFAULT_CC = 'cl.exe'

# Disable spammy warning related to av_restrict, upstream needs to fix.
DISABLED_WARNINGS = ['-wd4005']


def main():
  if len(sys.argv) < 3:
    print 'C99 to C89 Converter Wrapper'
    print '  usage: c99conv.py <input file> <output file> [-I <include> ...]'
    sys.exit(1)

  input_file = os.path.abspath(sys.argv[1])
  # Keep the preprocessed output file in the same directory so GOMA will work
  # without complaining about unknown paths.
  preprocessed_output_file = input_file + '_preprocessed.c'
  output_file = os.path.abspath(sys.argv[2])

  # Run the preprocessor command.  All of these settings are pulled from the
  # CFLAGS section of the "config.mak" created after running build_ffmpeg.sh.
  #
  # cl.exe is extremely inefficient (upwards of 30k writes) when asked to
  # preprocess to file (-P) so instead ask for output to go to stdout (-E) and
  # write it out ourselves afterward.  See http://crbug.com/172368.
  p = subprocess.Popen(
      [DEFAULT_CC, '-E', '-nologo', '-DCOMPILING_avcodec=1',
       '-DCOMPILING_avutil=1', '-DCOMPILING_avformat=1', '-D_USE_MATH_DEFINES',
       '-Dinline=__inline', '-Dstrtoll=_strtoi64', '-U__STRICT_ANSI__',
       '-D_ISOC99_SOURCE', '-D_LARGEFILE_SOURCE', '-DHAVE_AV_CONFIG_H',
       '-Dstrtod=avpriv_strtod', '-Dsnprintf=avpriv_snprintf',
       '-D_snprintf=avpriv_snprintf', '-Dvsnprintf=avpriv_vsnprintf',
       '-FIstdlib.h'] +
      sys.argv[3:] +
      DISABLED_WARNINGS +
      ['-I', '.', '-I', FFMPEG_ROOT, '-I', 'chromium/config',
       '-I', 'chromium/include/win', input_file],
      cwd=FFMPEG_ROOT, stderr=subprocess.STDOUT, stdout=subprocess.PIPE,
      # universal_newlines ensures whitespace is correct for #line directives.
      universal_newlines=True)
  stdout, stderr = p.communicate()

  # Abort if any error occurred.
  if p.returncode != 0:
    print stdout, stderr
    if os.path.isfile(preprocessed_output_file):
      os.unlink(preprocessed_output_file)
    sys.exit(p.returncode)

  with open(preprocessed_output_file, 'w') as f:
    # Write out stdout but skip the filename print out that MSVC forces for
    # every cl.exe execution as well as ridiculous amounts of white space; saves
    # ~64mb of output over the entire conversion!  Care must be taken while
    # trimming whitespace to keep #line directives accurate or stack traces will
    # be inaccurate.
    f.write(re.sub('\s+\n(#line|#pragma)', r'\n\1',
                   stdout[len(os.path.basename(input_file)):]))

  # Run the converter command.  Note: the input file must have a '.c' extension
  # or the converter will crash.  libclang does some funky detection based on
  # the file extension.
  p = subprocess.Popen(
      [CONVERTER_EXECUTABLE, preprocessed_output_file, output_file],
      cwd=FFMPEG_ROOT)
  p.wait()
  os.unlink(preprocessed_output_file)
  sys.exit(p.returncode)


if __name__ == '__main__':
  main()
