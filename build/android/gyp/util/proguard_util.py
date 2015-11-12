# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from util import build_utils

def FilterProguardOutput(output):
  '''ProGuard outputs boring stuff to stdout (proguard version, jar path, etc)
  as well as interesting stuff (notes, warnings, etc). If stdout is entirely
  boring, this method suppresses the output.
  '''
  ignore_patterns = [
    'ProGuard, version ',
    'Reading program jar [',
    'Reading library jar [',
    'Preparing output jar [',
    '  Copying resources from program jar [',
  ]
  for line in output.splitlines():
    for pattern in ignore_patterns:
      if line.startswith(pattern):
        break
    else:
      # line doesn't match any of the patterns; it's probably something worth
      # printing out.
      return output
  return ''


class ProguardCmdBuilder(object):
  def __init__(self, proguard_jar):
    assert os.path.exists(proguard_jar)
    self._proguard_jar_path = proguard_jar
    self._tested_apk_info_path = None
    self._tested_apk_info = None
    self._mapping = None
    self._libraries = None
    self._injars = None
    self._configs = None
    self._outjar = None
    self._cmd = None

  def outjar(self, path):
    assert self._cmd is None
    assert self._outjar is None
    self._outjar = path

  def tested_apk_info(self, tested_apk_info_path):
    assert self._cmd is None
    assert self._tested_apk_info is None
    self._tested_apk_info_path = tested_apk_info_path

  def mapping(self, path):
    assert self._cmd is None
    assert self._mapping is None
    assert os.path.exists(path), path
    self._mapping = path

  def libraryjars(self, paths):
    assert self._cmd is None
    assert self._libraries is None
    for p in paths:
      assert os.path.exists(p), p
    self._libraries = paths

  def injars(self, paths):
    assert self._cmd is None
    assert self._injars is None
    for p in paths:
      assert os.path.exists(p), p
    self._injars = paths

  def configs(self, paths):
    assert self._cmd is None
    assert self._configs is None
    for p in paths:
      assert os.path.exists(p), p
    self._configs = paths

  def build(self):
    if self._cmd:
      return self._cmd
    assert self._injars is not None
    assert self._outjar is not None
    assert self._configs is not None
    cmd = [
      'java', '-jar', self._proguard_jar_path,
      '-forceprocessing',
    ]
    if self._tested_apk_info_path:
      assert len(self._configs) == 1
      tested_apk_info = build_utils.ReadJson(self._tested_apk_info_path)
      self._configs += tested_apk_info['configs']
      self._injars = [
          p for p in self._injars if not p in tested_apk_info['inputs']]
      if not self._libraries:
        self._libraries = []
      self._libraries += tested_apk_info['inputs']
      self._mapping = tested_apk_info['mapping']
      cmd += [
        '-dontobfuscate',
        '-dontoptimize',
        '-dontshrink',
        '-dontskipnonpubliclibraryclassmembers',
      ]

    if self._mapping:
      cmd += [
        '-applymapping', self._mapping,
      ]

    if self._libraries:
      cmd += [
        '-libraryjars', ':'.join(self._libraries),
      ]

    cmd += [
      '-injars', ':'.join(self._injars)
    ]

    for config_file in self._configs:
      cmd += ['-include', config_file]

    # The output jar must be specified after inputs.
    cmd += [
      '-outjars', self._outjar,
      '-dump', self._outjar + '.dump',
      '-printseeds', self._outjar + '.seeds',
      '-printusage', self._outjar + '.usage',
      '-printmapping', self._outjar + '.mapping',
    ]
    self._cmd = cmd
    return self._cmd

  def GetInputs(self):
    self.build()
    inputs = [self._proguard_jar_path] + self._configs + self._injars
    if self._mapping:
      inputs.append(self._mapping)
    if self._libraries:
      inputs += self._libraries
    if self._tested_apk_info_path:
      inputs += [self._tested_apk_info_path]
    return inputs


  def CheckOutput(self):
    self.build()
    # Proguard will skip writing these files if they would be empty. Create
    # empty versions of them all now so that they are updated as the build
    # expects.
    open(self._outjar + '.dump', 'w').close()
    open(self._outjar + '.seeds', 'w').close()
    open(self._outjar + '.usage', 'w').close()
    open(self._outjar + '.mapping', 'w').close()
    build_utils.CheckOutput(self._cmd, print_stdout=True,
                            stdout_filter=FilterProguardOutput)

    this_info = {
      'inputs': self._injars,
      'configs': self._configs,
      'mapping': self._outjar + '.mapping',
    }

    build_utils.WriteJson(this_info, self._outjar + '.info')

