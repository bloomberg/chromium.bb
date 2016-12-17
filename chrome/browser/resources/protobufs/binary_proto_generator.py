#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
 Converts a given ASCII proto into a binary resource.

"""

import abc
import optparse
import os
import re
import subprocess
import sys
import traceback

class BinaryProtoGenerator:

  # If the script is run in a virtualenv
  # (https://virtualenv.pypa.io/en/stable/), then no google.protobuf library
  # should be brought in from site-packages. Passing -S into the interpreter in
  # a virtualenv actually destroys the ability to import standard library
  # functions like optparse, so this script should not be wrapped if we're in a
  # virtualenv.
  def _IsInVirtualEnv(self):
    # This is the way used by pip and other software to detect virtualenv.
    return hasattr(sys, 'real_prefix')

  def _ImportProtoModules(self, paths):
    """Import the protobuf modules we need. |paths| is list of import paths"""
    for path in paths:
      # Put the path to our proto libraries in front, so that we don't use
      # system protobuf.
      sys.path.insert(1, path)

    import google.protobuf.text_format as text_format
    globals()['text_format'] = text_format
    self.ImportProtoModule()

  def _GenerateBinaryProtos(self, opts):
    """ Read the ASCII proto and generate one or more binary protos. """
    # Read the ASCII
    with open(opts.infile, 'r') as ifile:
      ascii_pb_str = ifile.read()

    # Parse it into a structured PB
    full_pb = self.EmptyProtoInstance()
    text_format.Merge(ascii_pb_str, full_pb)

    self.ValidatePb(opts, full_pb);
    self.ProcessPb(opts, full_pb)

  @abc.abstractmethod
  def ImportProtoModule(self):
    """ Import the proto module to be used by the generator. """
    pass

  @abc.abstractmethod
  def EmptyProtoInstance(self):
    """ Returns an empty proto instance to be filled by the generator."""
    pass

  @abc.abstractmethod
  def ValidatePb(self, opts, pb):
    """ Validate the basic values of the protobuf.  The
        file_type_policies_unittest.cc will also validate it by platform,
        but this will catch errors earlier.
    """
    pass

  @abc.abstractmethod
  def ProcessPb(self, opts, pb):
    """ Process the parsed prototobuf. """
    pass

  def AddCommandLineOptions(self, parser):
    """ Allows subclasses to add any options the command line parser. """
    pass

  def AddExtraCommandLineArgsForVirtualEnvRun(self, opts, command):
    """ Allows subclasses to add any extra command line arguments when running
        this under a virtualenv."""
    pass

  def VerifyArgs(self, opts):
    """ Allows subclasses to check command line parameters before running. """
    return True

  def Run(self):
    parser = optparse.OptionParser()
    # TODO(crbug.com/614082): Remove this once the bug is fixed.
    parser.add_option('-w', '--wrap', action="store_true", default=False,
                      help='Wrap this script in another python '
                      'execution to disable site-packages.  This is a '
                      'fix for http://crbug.com/605592')

    parser.add_option('-i', '--infile',
                      help='The ASCII proto file to read.')
    parser.add_option('-d', '--outdir',
                      help='Directory underwhich binary file(s) will be ' +
                           'written')
    parser.add_option('-o', '--outbasename',
                      help='Basename of the binary file to write to.')
    parser.add_option('-p', '--path', action="append",
                      help='Repeat this as needed.  Directory(s) containing ' +
                      'the your_proto_definition_pb2.py and ' +
                      'google.protobuf.text_format modules')
    self.AddCommandLineOptions(parser)

    (opts, args) = parser.parse_args()
    if opts.infile is None or opts.outdir is None or opts.outbasename is None:
      parser.print_help()
      return 1

    if opts.wrap and not self._IsInVirtualEnv():
      # Run this script again with different args to the interpreter to suppress
      # the inclusion of libraries, like google.protobuf, from site-packages,
      # which is checked before sys.path when resolving imports. We want to
      # specifically import the libraries injected into the sys.path in
      # ImportProtoModules().
      command = [sys.executable, '-S', '-s', sys.argv[0]]
      command += ['-i', opts.infile]
      command += ['-d', opts.outdir]
      command += ['-o', opts.outbasename]
      for path in opts.path:
        command += ['-p', path]

      self.AddExtraCommandLineArgsForVirtualEnvRun(opts, command);
      sys.exit(subprocess.call(command))

    self._ImportProtoModules(opts.path)

    if not self.VerifyArgs(opts):
      print "Wrong arguments"
      return 1

    try:
      self._GenerateBinaryProtos(opts)
    except Exception as e:
      print "ERROR: Failed to render binary version of %s:\n  %s\n%s" % (
          opts.infile, str(e), traceback.format_exc())
      return 1
