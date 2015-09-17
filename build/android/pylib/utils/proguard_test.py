# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from pylib.utils import proguard

class TestParse(unittest.TestCase):

  def testClass(self):
    actual = proguard.Parse(
      ['- Program class: org/example/Test',
       '  Superclass: java/lang/Object'])
    expected = {
      'classes': [
        {
          'class': 'org.example.Test',
          'superclass': 'java.lang.Object',
          'annotations': {},
          'methods': []
        }
      ]
    }
    self.assertEquals(expected, actual)

  def testMethod(self):
    actual = proguard.Parse(
      ['- Program class: org/example/Test',
       'Methods (count = 1):',
       '- Method:       <init>()V'])
    expected = {
      'classes': [
        {
          'class': 'org.example.Test',
          'superclass': '',
          'annotations': {},
          'methods': [
            {
              'method': '<init>',
              'annotations': {}
            }
          ]
        }
      ]
    }
    self.assertEquals(expected, actual)

  def testClassAnnotation(self):
    actual = proguard.Parse(
      ['- Program class: org/example/Test',
       '  - Annotation [Lorg/example/Annotation;]:',
       '  - Annotation [Lorg/example/AnnotationWithValue;]:',
       '    - Constant element value [attr \'13\']',
       '      - Utf8 [val]'])
    expected = {
      'classes': [
        {
          'class': 'org.example.Test',
          'superclass': '',
          'annotations': {
            'Annotation': None,
            'AnnotationWithValue': 'val'
          },
          'methods': []
        }
      ]
    }
    self.assertEquals(expected, actual)

  def testMethodAnnotation(self):
    actual = proguard.Parse(
      ['- Program class: org/example/Test',
       'Methods (count = 1):',
       '- Method:       Test()V',
       '  - Annotation [Lorg/example/Annotation;]:',
       '  - Annotation [Lorg/example/AnnotationWithValue;]:',
       '    - Constant element value [attr \'13\']',
       '      - Utf8 [val]'])
    expected = {
      'classes': [
        {
          'class': 'org.example.Test',
          'superclass': '',
          'annotations': {},
          'methods': [
            {
              'method': 'Test',
              'annotations': {
                'Annotation': None,
                'AnnotationWithValue': 'val'
              },
            }
          ]
        }
      ]
    }
    self.assertEquals(expected, actual)
