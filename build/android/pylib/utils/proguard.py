# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import tempfile

from devil.utils import cmd_helper
from pylib import constants


_PROGUARD_CLASS_RE = re.compile(r'\s*?- Program class:\s*([\S]+)$')
_PROGUARD_SUPERCLASS_RE = re.compile(r'\s*?  Superclass:\s*([\S]+)$')
_PROGUARD_SECTION_RE = re.compile(
    r'^(?:Interfaces|Constant Pool|Fields|Methods|Class file attributes) '
    r'\(count = \d+\):$')
_PROGUARD_METHOD_RE = re.compile(r'\s*?- Method:\s*(\S*)[(].*$')
_PROGUARD_ANNOTATION_RE = re.compile(r'\s*?- Annotation \[L(\S*);\]:$')
_PROGUARD_ANNOTATION_CONST_RE = (
    re.compile(r'\s*?- Constant element value \[(\S*) .*\]$'))
_PROGUARD_ANNOTATION_ARRAY_RE = (
    re.compile(r'\s*?- Array element value \[(\S*)\]:$'))
_PROGUARD_ANNOTATION_VALUE_RE = re.compile(r'\s*?- \S+? \[(.*)\]$')

_PROGUARD_PATH_SDK = os.path.join(
    constants.PROGUARD_ROOT, 'lib', 'proguard.jar')
_PROGUARD_PATH_BUILT = (
    os.path.join(os.environ['ANDROID_BUILD_TOP'], 'external', 'proguard',
                 'lib', 'proguard.jar')
    if 'ANDROID_BUILD_TOP' in os.environ else None)
_PROGUARD_PATH = (
    _PROGUARD_PATH_SDK if os.path.exists(_PROGUARD_PATH_SDK)
    else _PROGUARD_PATH_BUILT)


def Dump(jar_path):
  """Dumps class and method information from a JAR into a dict via proguard.

  Args:
    jar_path: An absolute path to the JAR file to dump.
  Returns:
    A dict in the following format:
      {
        'classes': [
          {
            'class': '',
            'superclass': '',
            'annotations': {/* dict -- see below */},
            'methods': [
              {
                'method': '',
                'annotations': {/* dict -- see below */},
              },
              ...
            ],
          },
          ...
        ],
      }

    Annotations dict format:
      {
        'empty-annotation-class-name': None,
        'annotation-class-name': {
          'field': 'primitive-value',
          'field': [ 'array-item-1', 'array-item-2', ... ],
          /* Object fields are not supported yet, coming soon! */
          'field': {
            /* Object value */
            'field': 'primitive-value',
            'field': [ 'array-item-1', 'array-item-2', ... ],
            'field': { /* Object value */ }
          }
        }
      }

    Note that for top-level annotations their class names are used for
    identification, whereas for any nested annotations the corresponding
    field names are used.
  """

  with tempfile.NamedTemporaryFile() as proguard_output:
    cmd_helper.RunCmd(['java', '-jar',
                       _PROGUARD_PATH,
                       '-injars', jar_path,
                       '-dontshrink',
                       '-dontoptimize',
                       '-dontobfuscate',
                       '-dontpreverify',
                       '-dump', proguard_output.name])
    return Parse(proguard_output)

class _ParseState(object):
  def __init__(self):
    self.class_result = None
    self.method_result = None
    self.annotation = None
    self.annotation_field = None

  def ResetPerSection(self):
    self.InitMethod(None)

  def InitClass(self, class_result):
    self.InitMethod(None)
    self.class_result = class_result

  def InitMethod(self, method_result):
    self.annotation = None
    self.ResetAnnotationField()
    self.method_result = method_result
    if method_result:
      self.class_result['methods'].append(method_result)

  def InitAnnotation(self, annotation):
    self.annotation = annotation
    self.GetAnnotations()[annotation] = None
    self.ResetAnnotationField()

  def ResetAnnotationField(self):
    self.annotation_field = None

  def InitAnnotationField(self, field, value):
    if not self.GetCurrentAnnotation():
      self.GetAnnotations()[self.annotation] = {}
    self.GetCurrentAnnotation()[field] = value
    self.annotation_field = field

  def UpdateCurrentAnnotationField(self, value):
    ann = self.GetCurrentAnnotation()
    assert ann
    if type(ann[self.annotation_field]) is list:
      ann[self.annotation_field].append(value)
    else:
      ann[self.annotation_field] = value
      self.ResetAnnotationField()

  def GetAnnotations(self):
    if self.method_result:
      return self.method_result['annotations']
    else:
      return self.class_result['annotations']

  def GetCurrentAnnotation(self):
    assert self.annotation
    return self.GetAnnotations()[self.annotation]

def Parse(proguard_output):
  results = {
    'classes': [],
  }

  state = _ParseState()

  for line in proguard_output:
    line = line.strip('\r\n')

    m = _PROGUARD_CLASS_RE.match(line)
    if m:
      class_result = {
        'class': m.group(1).replace('/', '.'),
        'superclass': '',
        'annotations': {},
        'methods': [],
      }
      results['classes'].append(class_result)
      state.InitClass(class_result)
      continue

    if not state.class_result:
      continue

    m = _PROGUARD_SUPERCLASS_RE.match(line)
    if m:
      state.class_result['superclass'] = m.group(1).replace('/', '.')
      continue

    m = _PROGUARD_SECTION_RE.match(line)
    if m:
      state.ResetPerSection()
      continue

    m = _PROGUARD_METHOD_RE.match(line)
    if m:
      state.InitMethod({
        'method': m.group(1),
        'annotations': {},
      })
      continue

    m = _PROGUARD_ANNOTATION_RE.match(line)
    if m:
      # Ignore the annotation package.
      state.InitAnnotation(m.group(1).split('/')[-1])
      continue

    if state.annotation:
      if state.annotation_field:
        m = _PROGUARD_ANNOTATION_VALUE_RE.match(line)
        if m:
          state.UpdateCurrentAnnotationField(m.group(1))
          continue
        m = _PROGUARD_ANNOTATION_CONST_RE.match(line)
        if m:
          if not m.group(1) == '(default)':
            state.ResetAnnotationField()
        else:
          m = _PROGUARD_ANNOTATION_ARRAY_RE.match(line)
          if m:
            state.ResetAnnotationField()
      if not state.annotation_field:
        m = _PROGUARD_ANNOTATION_CONST_RE.match(line)
        if m:
          state.InitAnnotationField(m.group(1), None)
          continue
        m = _PROGUARD_ANNOTATION_ARRAY_RE.match(line)
        if m:
          state.InitAnnotationField(m.group(1), [])
  return results
