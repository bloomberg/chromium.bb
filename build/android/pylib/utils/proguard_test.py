# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from pylib.utils import proguard

class TestParse(unittest.TestCase):

  def setUp(self):
    self.maxDiff = None

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
       '      - Utf8 [val]',
       '  - Annotation [Lorg/example/AnnotationWithTwoValues;]:',
       '    - Constant element value [attr1 \'13\']',
       '      - Utf8 [val1]',
       '    - Constant element value [attr2 \'13\']',
       '      - Utf8 [val2]'])
    expected = {
      'classes': [
        {
          'class': 'org.example.Test',
          'superclass': '',
          'annotations': {
            'Annotation': None,
            'AnnotationWithValue': {'attr': 'val'},
            'AnnotationWithTwoValues': {'attr1': 'val1', 'attr2': 'val2'}
          },
          'methods': []
        }
      ]
    }
    self.assertEquals(expected, actual)

  def testClassAnnotationWithArrays(self):
    actual = proguard.Parse(
      ['- Program class: org/example/Test',
       '  - Annotation [Lorg/example/AnnotationWithEmptyArray;]:',
       '    - Array element value [arrayAttr]:',
       '  - Annotation [Lorg/example/AnnotationWithOneElemArray;]:',
       '    - Array element value [arrayAttr]:',
       '      - Constant element value [(default) \'13\']',
       '        - Utf8 [val]',
       '  - Annotation [Lorg/example/AnnotationWithTwoElemArray;]:',
       '    - Array element value [arrayAttr]:',
       '      - Constant element value [(default) \'13\']',
       '        - Utf8 [val1]',
       '      - Constant element value [(default) \'13\']',
       '        - Utf8 [val2]'])
    expected = {
      'classes': [
        {
          'class': 'org.example.Test',
          'superclass': '',
          'annotations': {
            'AnnotationWithEmptyArray': {'arrayAttr': []},
            'AnnotationWithOneElemArray': {'arrayAttr': ['val']},
            'AnnotationWithTwoElemArray': {'arrayAttr': ['val1', 'val2']}
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
       '      - Utf8 [val]',
       '  - Annotation [Lorg/example/AnnotationWithTwoValues;]:',
       '    - Constant element value [attr1 \'13\']',
       '      - Utf8 [val1]',
       '    - Constant element value [attr2 \'13\']',
       '      - Utf8 [val2]'])
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
                'AnnotationWithValue': {'attr': 'val'},
                'AnnotationWithTwoValues': {'attr1': 'val1', 'attr2': 'val2'}
              },
            }
          ]
        }
      ]
    }
    self.assertEquals(expected, actual)

  def testMethodAnnotationWithArrays(self):
    actual = proguard.Parse(
      ['- Program class: org/example/Test',
       'Methods (count = 1):',
       '- Method:       Test()V',
       '  - Annotation [Lorg/example/AnnotationWithEmptyArray;]:',
       '    - Array element value [arrayAttr]:',
       '  - Annotation [Lorg/example/AnnotationWithOneElemArray;]:',
       '    - Array element value [arrayAttr]:',
       '      - Constant element value [(default) \'13\']',
       '        - Utf8 [val]',
       '  - Annotation [Lorg/example/AnnotationWithTwoElemArray;]:',
       '    - Array element value [arrayAttr]:',
       '      - Constant element value [(default) \'13\']',
       '        - Utf8 [val1]',
       '      - Constant element value [(default) \'13\']',
       '        - Utf8 [val2]'])
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
                'AnnotationWithEmptyArray': {'arrayAttr': []},
                'AnnotationWithOneElemArray': {'arrayAttr': ['val']},
                'AnnotationWithTwoElemArray': {'arrayAttr': ['val1', 'val2']}
              },
            }
          ]
        }
      ]
    }
    self.assertEquals(expected, actual)

  def testMethodAnnotationWithPrimitivesAndArrays(self):
    actual = proguard.Parse(
      ['- Program class: org/example/Test',
       'Methods (count = 1):',
       '- Method:       Test()V',
       '  - Annotation [Lorg/example/AnnotationPrimitiveThenArray;]:',
       '    - Constant element value [attr \'13\']',
       '      - Utf8 [val]',
       '    - Array element value [arrayAttr]:',
       '      - Constant element value [(default) \'13\']',
       '        - Utf8 [val]',
       '  - Annotation [Lorg/example/AnnotationArrayThenPrimitive;]:',
       '    - Array element value [arrayAttr]:',
       '      - Constant element value [(default) \'13\']',
       '        - Utf8 [val]',
       '    - Constant element value [attr \'13\']',
       '      - Utf8 [val]',
       '  - Annotation [Lorg/example/AnnotationTwoArrays;]:',
       '    - Array element value [arrayAttr1]:',
       '      - Constant element value [(default) \'13\']',
       '        - Utf8 [val1]',
       '    - Array element value [arrayAttr2]:',
       '      - Constant element value [(default) \'13\']',
       '        - Utf8 [val2]'])
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
                'AnnotationPrimitiveThenArray': {'attr': 'val',
                                                 'arrayAttr': ['val']},
                'AnnotationArrayThenPrimitive': {'arrayAttr': ['val'],
                                                 'attr': 'val'},
                'AnnotationTwoArrays': {'arrayAttr1': ['val1'],
                                        'arrayAttr2': ['val2']}
              },
            }
          ]
        }
      ]
    }
    self.assertEquals(expected, actual)
