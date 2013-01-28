#!/usr/bin/python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from ctypes import windll, wintypes
import os
import sys
import subprocess


# Path to root FFmpeg directory.  Used as the CWD for executing all commands.
FFMPEG_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..'))

# Path to the C99 to C89 converter.
CONVERTER_EXECUTABLE = os.path.abspath(os.path.join(
    FFMPEG_ROOT, 'chromium', 'binaries', 'c99conv.exe'))

# $CC to use if GOMA can't be detected.
DEFAULT_CC = ['cl.exe']

# Disable spammy warning related to av_restrict, upstream needs to fix.
DISABLED_WARNINGS = ['-wd4005']


# Figure out if GOMA is available or not.  Kind of hacky, but well worth it
# since this will cut the run time from ~3 minutes to ~30 seconds (-j 256).
def GetCC():
  # Things that don't work:
  #   - Checking for $CC, not set and seems to be explicitly removed by gyp.
  #   - Trying to find include.gypi, as with $CC, the script is called with a
  #     sanitized environment which removes $USERPROFILE.

  # See if the user has a chromium.gyp_env setting for GOMA.
  gyp_env = os.path.abspath(os.path.join(
      FFMPEG_ROOT, '..', '..', '..', 'chromium.gyp_env'))
  if os.path.isfile(gyp_env):
    gyp_config = eval(open(gyp_env, 'r').read())
    if 'CC' in gyp_config:
      return gyp_config['CC'].split()

  # See if the user's build.ninja uses GOMA.
  for config_name in ('Release', 'Debug'):
    build_ninja = os.path.abspath(os.path.join(
        FFMPEG_ROOT, '..', '..', 'out', config_name, 'build.ninja'))
    if os.path.isfile(build_ninja):
      with open(build_ninja) as f:
        for line in f:
          if line.startswith('cc ='):
            return line.rstrip().replace('cc =', '').split()

  return DEFAULT_CC


# TODO(scottmg|dalecurtis|iannucci): Replace this with gyp 'pool' syntax when
# available.
class PreprocessLock(object):
  """A flock-style lock to limit the number of concurrent preprocesses to one
  when running locally. Not used when using goma to preprocess.

  Cuts local cl.exe time down from ~3-4m to ~1m.

  Uses a session-local mutex based on the file's directory.
  """
  def __init__(self, cc):
    self.cc = cc

  def __enter__(self):
    # Only lock if we're using the default CC, not when using goma.
    if self.cc != DEFAULT_CC:
      return
    name = 'Local\\c99conv_cl_preprocess_mutex'
    self.mutex = windll.kernel32.CreateMutexW(
        wintypes.c_int(0),
        wintypes.c_int(0),
        wintypes.create_unicode_buffer(name))
    assert self.mutex
    result = windll.kernel32.WaitForSingleObject(
        self.mutex, wintypes.c_int(0xFFFFFFFF))
    # 0x80 means another process was killed without releasing the mutex, but
    # that this process has been given ownership. This is fine for our
    # purposes.
    assert result in (0, 0x80), (
        "%s, %s" % (result, windll.kernel32.GetLastError()))

  def __exit__(self, type, value, traceback):
    if self.cc == DEFAULT_CC:
      windll.kernel32.ReleaseMutex(self.mutex)
      windll.kernel32.CloseHandle(self.mutex)


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

  # Find $CC, hope for GOMA.
  cc = GetCC()

  with PreprocessLock(cc):
    # Run the preprocessor command.  All of these settings are pulled from the
    # CFLAGS section of the "config.mak" created after running build_ffmpeg.sh.
    p = subprocess.Popen(
        cc + ['-P', '-nologo', '-DCOMPILING_avcodec=1', '-DCOMPILING_avutil=1',
              '-DCOMPILING_avformat=1', '-D_USE_MATH_DEFINES',
              '-Dinline=__inline', '-Dstrtoll=_strtoi64', '-U__STRICT_ANSI__',
              '-D_ISOC99_SOURCE', '-D_LARGEFILE_SOURCE', '-DHAVE_AV_CONFIG_H',
              '-Dstrtod=avpriv_strtod', '-Dsnprintf=avpriv_snprintf',
              '-D_snprintf=avpriv_snprintf', '-Dvsnprintf=avpriv_vsnprintf',
              '-FIstdlib.h'] + sys.argv[3:] +
             DISABLED_WARNINGS +
             ['-I', '.', '-I', FFMPEG_ROOT, '-I', 'chromium/config',
              '-I', 'chromium/include/win',
              '-Fi%s' % preprocessed_output_file, input_file],
        cwd=FFMPEG_ROOT, stderr=subprocess.STDOUT, stdout=subprocess.PIPE)
    stdout, _ = p.communicate()

  # Print all lines if an error occurred, otherwise skip filename print out that
  # MSVC forces for every cl.exe execution.
  for line in stdout.splitlines():
    if p.returncode != 0 or not os.path.basename(input_file) == line.strip():
      print line

  # Abort if any error occurred.
  if p.returncode != 0:
    if os.path.isfile(preprocessed_output_file):
      os.unlink(preprocessed_output_file)
    sys.exit(p.returncode)

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
