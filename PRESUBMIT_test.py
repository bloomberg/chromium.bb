#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import re
import subprocess
import unittest

import PRESUBMIT
from PRESUBMIT_test_mocks import MockChange, MockFile, MockAffectedFile
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi

_TEST_DATA_DIR = 'base/test/data/presubmit'

class VersionControlConflictsTest(unittest.TestCase):
  def testTypicalConflict(self):
    lines = ['<<<<<<< HEAD',
             '  base::ScopedTempDir temp_dir_;',
             '=======',
             '  ScopedTempDir temp_dir_;',
             '>>>>>>> master']
    errors = PRESUBMIT._CheckForVersionControlConflictsInFile(
        MockInputApi(), MockFile('some/path/foo_platform.cc', lines))
    self.assertEqual(3, len(errors))
    self.assertTrue('1' in errors[0])
    self.assertTrue('3' in errors[1])
    self.assertTrue('5' in errors[2])

  def testIgnoresReadmes(self):
    lines = ['A First Level Header',
             '====================',
             '',
             'A Second Level Header',
             '---------------------']
    errors = PRESUBMIT._CheckForVersionControlConflictsInFile(
        MockInputApi(), MockFile('some/polymer/README.md', lines))
    self.assertEqual(0, len(errors))

class UmaHistogramChangeMatchedOrNotTest(unittest.TestCase):
  def testTypicalCorrectlyMatchedChange(self):
    diff_cc = ['UMA_HISTOGRAM_BOOL("Bla.Foo.Dummy", true)']
    diff_xml = ['<histogram name="Bla.Foo.Dummy"> </histogram>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('tools/metrics/histograms/histograms.xml', diff_xml),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testTypicalNotMatchedChange(self):
    diff_cc = ['UMA_HISTOGRAM_BOOL("Bla.Foo.Dummy", true)']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('some/path/foo.cc', diff_cc)]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual('warning', warnings[0].type)

  def testTypicalNotMatchedChangeViaSuffixes(self):
    diff_cc = ['UMA_HISTOGRAM_BOOL("Bla.Foo.Dummy", true)']
    diff_xml = ['<histogram_suffixes name="SuperHistogram">',
                '  <suffix name="Dummy"/>',
                '  <affected-histogram name="Snafu.Dummy"/>',
                '</histogram>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('tools/metrics/histograms/histograms.xml', diff_xml),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual('warning', warnings[0].type)

  def testTypicalCorrectlyMatchedChangeViaSuffixes(self):
    diff_cc = ['UMA_HISTOGRAM_BOOL("Bla.Foo.Dummy", true)']
    diff_xml = ['<histogram_suffixes name="SuperHistogram">',
                '  <suffix name="Dummy"/>',
                '  <affected-histogram name="Bla.Foo"/>',
                '</histogram>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('tools/metrics/histograms/histograms.xml', diff_xml),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testTypicalCorrectlyMatchedChangeViaSuffixesWithSeparator(self):
    diff_cc = ['UMA_HISTOGRAM_BOOL("Snafu_Dummy", true)']
    diff_xml = ['<histogram_suffixes name="SuperHistogram" separator="_">',
                '  <suffix name="Dummy"/>',
                '  <affected-histogram name="Snafu"/>',
                '</histogram>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('tools/metrics/histograms/histograms.xml', diff_xml),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(0, len(warnings))

class BadExtensionsTest(unittest.TestCase):
  def testBadRejFile(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', ''),
      MockFile('some/path/foo.cc.rej', ''),
      MockFile('some/path2/bar.h.rej', ''),
    ]

    results = PRESUBMIT._CheckPatchFiles(mock_input_api, MockOutputApi())
    self.assertEqual(1, len(results))
    self.assertEqual(2, len(results[0].items))
    self.assertTrue('foo.cc.rej' in results[0].items[0])
    self.assertTrue('bar.h.rej' in results[0].items[1])

  def testBadOrigFile(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('other/path/qux.h.orig', ''),
      MockFile('other/path/qux.h', ''),
      MockFile('other/path/qux.cc', ''),
    ]

    results = PRESUBMIT._CheckPatchFiles(mock_input_api, MockOutputApi())
    self.assertEqual(1, len(results))
    self.assertEqual(1, len(results[0].items))
    self.assertTrue('qux.h.orig' in results[0].items[0])

  def testGoodFiles(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('other/path/qux.h', ''),
      MockFile('other/path/qux.cc', ''),
    ]
    results = PRESUBMIT._CheckPatchFiles(mock_input_api, MockOutputApi())
    self.assertEqual(0, len(results))


class CheckSingletonInHeadersTest(unittest.TestCase):
  def testSingletonInArbitraryHeader(self):
    diff_singleton_h = ['base::subtle::AtomicWord '
                        'base::Singleton<Type, Traits, DifferentiatingType>::']
    diff_foo_h = ['// base::Singleton<Foo> in comment.',
                  'friend class base::Singleton<Foo>']
    diff_foo2_h = ['  //Foo* bar = base::Singleton<Foo>::get();']
    diff_bad_h = ['Foo* foo = base::Singleton<Foo>::get();']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockAffectedFile('base/memory/singleton.h',
                                     diff_singleton_h),
                            MockAffectedFile('foo.h', diff_foo_h),
                            MockAffectedFile('foo2.h', diff_foo2_h),
                            MockAffectedFile('bad.h', diff_bad_h)]
    warnings = PRESUBMIT._CheckSingletonInHeaders(mock_input_api,
                                                  MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(2, len(warnings[0].items))
    self.assertEqual('error', warnings[0].type)
    self.assertTrue('Found base::Singleton<T>' in warnings[0].message)

  def testSingletonInCC(self):
    diff_cc = ['Foo* foo = base::Singleton<Foo>::get();']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockAffectedFile('some/path/foo.cc', diff_cc)]
    warnings = PRESUBMIT._CheckSingletonInHeaders(mock_input_api,
                                                  MockOutputApi())
    self.assertEqual(0, len(warnings))


class InvalidOSMacroNamesTest(unittest.TestCase):
  def testInvalidOSMacroNames(self):
    lines = ['#if defined(OS_WINDOWS)',
             ' #elif defined(OS_WINDOW)',
             ' # if defined(OS_MACOSX) || defined(OS_CHROME)',
             '# else  // defined(OS_MAC)',
             '#endif  // defined(OS_MACOS)']
    errors = PRESUBMIT._CheckForInvalidOSMacrosInFile(
        MockInputApi(), MockFile('some/path/foo_platform.cc', lines))
    self.assertEqual(len(lines), len(errors))
    self.assertTrue(':1 OS_WINDOWS' in errors[0])
    self.assertTrue('(did you mean OS_WIN?)' in errors[0])

  def testValidOSMacroNames(self):
    lines = ['#if defined(%s)' % m for m in PRESUBMIT._VALID_OS_MACROS]
    errors = PRESUBMIT._CheckForInvalidOSMacrosInFile(
        MockInputApi(), MockFile('some/path/foo_platform.cc', lines))
    self.assertEqual(0, len(errors))


class InvalidIfDefinedMacroNamesTest(unittest.TestCase):
  def testInvalidIfDefinedMacroNames(self):
    lines = ['#if defined(TARGET_IPHONE_SIMULATOR)',
             '#if !defined(TARGET_IPHONE_SIMULATOR)',
             '#elif defined(TARGET_IPHONE_SIMULATOR)',
             '#ifdef TARGET_IPHONE_SIMULATOR',
             ' # ifdef TARGET_IPHONE_SIMULATOR',
             '# if defined(VALID) || defined(TARGET_IPHONE_SIMULATOR)',
             '# else  // defined(TARGET_IPHONE_SIMULATOR)',
             '#endif  // defined(TARGET_IPHONE_SIMULATOR)',]
    errors = PRESUBMIT._CheckForInvalidIfDefinedMacrosInFile(
        MockInputApi(), MockFile('some/path/source.mm', lines))
    self.assertEqual(len(lines), len(errors))

  def testValidIfDefinedMacroNames(self):
    lines = ['#if defined(FOO)',
             '#ifdef BAR',]
    errors = PRESUBMIT._CheckForInvalidIfDefinedMacrosInFile(
        MockInputApi(), MockFile('some/path/source.cc', lines))
    self.assertEqual(0, len(errors))


class CheckAddedDepsHaveTetsApprovalsTest(unittest.TestCase):

  def calculate(self, old_include_rules, old_specific_include_rules,
                new_include_rules, new_specific_include_rules):
    return PRESUBMIT._CalculateAddedDeps(
        os.path, 'include_rules = %r\nspecific_include_rules = %r' % (
            old_include_rules, old_specific_include_rules),
        'include_rules = %r\nspecific_include_rules = %r' % (
            new_include_rules, new_specific_include_rules))

  def testCalculateAddedDeps(self):
    old_include_rules = [
        '+base',
        '-chrome',
        '+content',
        '-grit',
        '-grit/",',
        '+jni/fooblat.h',
        '!sandbox',
    ]
    old_specific_include_rules = {
        'compositor\.*': {
            '+cc',
        },
    }

    new_include_rules = [
        '-ash',
        '+base',
        '+chrome',
        '+components',
        '+content',
        '+grit',
        '+grit/generated_resources.h",',
        '+grit/",',
        '+jni/fooblat.h',
        '+policy',
        '+' + os.path.join('third_party', 'WebKit'),
    ]
    new_specific_include_rules = {
        'compositor\.*': {
            '+cc',
        },
        'widget\.*': {
            '+gpu',
        },
    }

    expected = set([
        os.path.join('chrome', 'DEPS'),
        os.path.join('gpu', 'DEPS'),
        os.path.join('components', 'DEPS'),
        os.path.join('policy', 'DEPS'),
        os.path.join('third_party', 'WebKit', 'DEPS'),
    ])
    self.assertEqual(
        expected,
        self.calculate(old_include_rules, old_specific_include_rules,
                       new_include_rules, new_specific_include_rules))

  def testCalculateAddedDepsIgnoresPermutations(self):
    old_include_rules = [
        '+base',
        '+chrome',
    ]
    new_include_rules = [
        '+chrome',
        '+base',
    ]
    self.assertEqual(set(),
                     self.calculate(old_include_rules, {}, new_include_rules,
                                    {}))


class JSONParsingTest(unittest.TestCase):
  def testSuccess(self):
    input_api = MockInputApi()
    filename = 'valid_json.json'
    contents = ['// This is a comment.',
                '{',
                '  "key1": ["value1", "value2"],',
                '  "key2": 3  // This is an inline comment.',
                '}'
                ]
    input_api.files = [MockFile(filename, contents)]
    self.assertEqual(None,
                     PRESUBMIT._GetJSONParseError(input_api, filename))

  def testFailure(self):
    input_api = MockInputApi()
    test_data = [
      ('invalid_json_1.json',
       ['{ x }'],
       'Expecting property name:'),
      ('invalid_json_2.json',
       ['// Hello world!',
        '{ "hello": "world }'],
       'Unterminated string starting at:'),
      ('invalid_json_3.json',
       ['{ "a": "b", "c": "d", }'],
       'Expecting property name:'),
      ('invalid_json_4.json',
       ['{ "a": "b" "c": "d" }'],
       'Expecting , delimiter:'),
      ]

    input_api.files = [MockFile(filename, contents)
                       for (filename, contents, _) in test_data]

    for (filename, _, expected_error) in test_data:
      actual_error = PRESUBMIT._GetJSONParseError(input_api, filename)
      self.assertTrue(expected_error in str(actual_error),
                      "'%s' not found in '%s'" % (expected_error, actual_error))

  def testNoEatComments(self):
    input_api = MockInputApi()
    file_with_comments = 'file_with_comments.json'
    contents_with_comments = ['// This is a comment.',
                              '{',
                              '  "key1": ["value1", "value2"],',
                              '  "key2": 3  // This is an inline comment.',
                              '}'
                              ]
    file_without_comments = 'file_without_comments.json'
    contents_without_comments = ['{',
                                 '  "key1": ["value1", "value2"],',
                                 '  "key2": 3',
                                 '}'
                                 ]
    input_api.files = [MockFile(file_with_comments, contents_with_comments),
                       MockFile(file_without_comments,
                                contents_without_comments)]

    self.assertEqual('No JSON object could be decoded',
                     str(PRESUBMIT._GetJSONParseError(input_api,
                                                      file_with_comments,
                                                      eat_comments=False)))
    self.assertEqual(None,
                     PRESUBMIT._GetJSONParseError(input_api,
                                                  file_without_comments,
                                                  eat_comments=False))


class IDLParsingTest(unittest.TestCase):
  def testSuccess(self):
    input_api = MockInputApi()
    filename = 'valid_idl_basics.idl'
    contents = ['// Tests a valid IDL file.',
                'namespace idl_basics {',
                '  enum EnumType {',
                '    name1,',
                '    name2',
                '  };',
                '',
                '  dictionary MyType1 {',
                '    DOMString a;',
                '  };',
                '',
                '  callback Callback1 = void();',
                '  callback Callback2 = void(long x);',
                '  callback Callback3 = void(MyType1 arg);',
                '  callback Callback4 = void(EnumType type);',
                '',
                '  interface Functions {',
                '    static void function1();',
                '    static void function2(long x);',
                '    static void function3(MyType1 arg);',
                '    static void function4(Callback1 cb);',
                '    static void function5(Callback2 cb);',
                '    static void function6(Callback3 cb);',
                '    static void function7(Callback4 cb);',
                '  };',
                '',
                '  interface Events {',
                '    static void onFoo1();',
                '    static void onFoo2(long x);',
                '    static void onFoo2(MyType1 arg);',
                '    static void onFoo3(EnumType type);',
                '  };',
                '};'
                ]
    input_api.files = [MockFile(filename, contents)]
    self.assertEqual(None,
                     PRESUBMIT._GetIDLParseError(input_api, filename))

  def testFailure(self):
    input_api = MockInputApi()
    test_data = [
      ('invalid_idl_1.idl',
       ['//',
        'namespace test {',
        '  dictionary {',
        '    DOMString s;',
        '  };',
        '};'],
       'Unexpected "{" after keyword "dictionary".\n'),
      # TODO(yoz): Disabled because it causes the IDL parser to hang.
      # See crbug.com/363830.
      # ('invalid_idl_2.idl',
      #  (['namespace test {',
      #    '  dictionary MissingSemicolon {',
      #    '    DOMString a',
      #    '    DOMString b;',
      #    '  };',
      #    '};'],
      #   'Unexpected symbol DOMString after symbol a.'),
      ('invalid_idl_3.idl',
       ['//',
        'namespace test {',
        '  enum MissingComma {',
        '    name1',
        '    name2',
        '  };',
        '};'],
       'Unexpected symbol name2 after symbol name1.'),
      ('invalid_idl_4.idl',
       ['//',
        'namespace test {',
        '  enum TrailingComma {',
        '    name1,',
        '    name2,',
        '  };',
        '};'],
       'Trailing comma in block.'),
      ('invalid_idl_5.idl',
       ['//',
        'namespace test {',
        '  callback Callback1 = void(;',
        '};'],
       'Unexpected ";" after "(".'),
      ('invalid_idl_6.idl',
       ['//',
        'namespace test {',
        '  callback Callback1 = void(long );',
        '};'],
       'Unexpected ")" after symbol long.'),
      ('invalid_idl_7.idl',
       ['//',
        'namespace test {',
        '  interace Events {',
        '    static void onFoo1();',
        '  };',
        '};'],
       'Unexpected symbol Events after symbol interace.'),
      ('invalid_idl_8.idl',
       ['//',
        'namespace test {',
        '  interface NotEvent {',
        '    static void onFoo1();',
        '  };',
        '};'],
       'Did not process Interface Interface(NotEvent)'),
      ('invalid_idl_9.idl',
       ['//',
        'namespace test {',
        '  interface {',
        '    static void function1();',
        '  };',
        '};'],
       'Interface missing name.'),
      ]

    input_api.files = [MockFile(filename, contents)
                       for (filename, contents, _) in test_data]

    for (filename, _, expected_error) in test_data:
      actual_error = PRESUBMIT._GetIDLParseError(input_api, filename)
      self.assertTrue(expected_error in str(actual_error),
                      "'%s' not found in '%s'" % (expected_error, actual_error))


class TryServerMasterTest(unittest.TestCase):
  def testTryServerMasters(self):
    bots = {
        'master.tryserver.chromium.android': [
            'android_archive_rel_ng',
            'android_arm64_dbg_recipe',
            'android_blink_rel',
            'android_chromium_variable',
            'android_chromium_variable_archive',
            'android_chromium_variable_arm64',
            'android_chromium_variable_cast_shell',
            'android_chromium_variable_clang',
            'android_chromium_variable_gn',
            'android_chromium_variable_nexus4',
            'android_clang_dbg_recipe',
            'android_compile_dbg',
            'android_compile_mips_dbg',
            'android_compile_rel',
            'android_compile_x64_dbg',
            'android_compile_x86_dbg',
            'android_coverage',
            'android_cronet_tester'
            'android_swarming_rel',
            'cast_shell_android',
            'linux_android_dbg_ng',
            'linux_android_rel_ng',
        ],
        'master.tryserver.chromium.mac': [
            'ios_dbg_simulator',
            'ios_rel_device',
            'ios_rel_device_ninja',
            'mac_asan',
            'mac_asan_64',
            'mac_chromium_compile_dbg',
            'mac_chromium_compile_rel',
            'mac_chromium_dbg',
            'mac_chromium_rel',
            'mac_nacl_sdk',
            'mac_nacl_sdk_build',
            'mac_rel_naclmore',
            'mac_x64_rel',
            'mac_xcodebuild',
        ],
        'master.tryserver.chromium.linux': [
            'chromium_presubmit',
            'linux_arm_cross_compile',
            'linux_arm_tester',
            'linux_chromeos_asan',
            'linux_chromeos_browser_asan',
            'linux_chromeos_valgrind',
            'linux_chromium_chromeos_dbg',
            'linux_chromium_chromeos_rel',
            'linux_chromium_compile_dbg',
            'linux_chromium_compile_rel',
            'linux_chromium_dbg',
            'linux_chromium_gn_dbg',
            'linux_chromium_gn_rel',
            'linux_chromium_rel',
            'linux_chromium_trusty32_dbg',
            'linux_chromium_trusty32_rel',
            'linux_chromium_trusty_dbg',
            'linux_chromium_trusty_rel',
            'linux_clang_tsan',
            'linux_ecs_ozone',
            'linux_layout',
            'linux_layout_asan',
            'linux_layout_rel',
            'linux_layout_rel_32',
            'linux_nacl_sdk',
            'linux_nacl_sdk_bionic',
            'linux_nacl_sdk_bionic_build',
            'linux_nacl_sdk_build',
            'linux_redux',
            'linux_rel_naclmore',
            'linux_rel_precise32',
            'linux_valgrind',
            'tools_build_presubmit',
        ],
        'master.tryserver.chromium.win': [
            'win8_aura',
            'win8_chromium_dbg',
            'win8_chromium_rel',
            'win_chromium_compile_dbg',
            'win_chromium_compile_rel',
            'win_chromium_dbg',
            'win_chromium_rel',
            'win_chromium_rel',
            'win_chromium_x64_dbg',
            'win_chromium_x64_rel',
            'win_nacl_sdk',
            'win_nacl_sdk_build',
            'win_rel_naclmore',
         ],
    }
    for master, bots in bots.iteritems():
      for bot in bots:
        self.assertEqual(master, PRESUBMIT.GetTryServerMasterForBot(bot),
                         'bot=%s: expected %s, computed %s' % (
            bot, master, PRESUBMIT.GetTryServerMasterForBot(bot)))


class UserMetricsActionTest(unittest.TestCase):
  def testUserMetricsActionInActions(self):
    input_api = MockInputApi()
    file_with_user_action = 'file_with_user_action.cc'
    contents_with_user_action = [
      'base::UserMetricsAction("AboutChrome")'
    ]

    input_api.files = [MockFile(file_with_user_action,
                                contents_with_user_action)]

    self.assertEqual(
      [], PRESUBMIT._CheckUserActionUpdate(input_api, MockOutputApi()))


  def testUserMetricsActionNotAddedToActions(self):
    input_api = MockInputApi()
    file_with_user_action = 'file_with_user_action.cc'
    contents_with_user_action = [
      'base::UserMetricsAction("NotInActionsXml")'
    ]

    input_api.files = [MockFile(file_with_user_action,
                                contents_with_user_action)]

    output = PRESUBMIT._CheckUserActionUpdate(input_api, MockOutputApi())
    self.assertEqual(
      ('File %s line %d: %s is missing in '
       'tools/metrics/actions/actions.xml. Please run '
       'tools/metrics/actions/extract_actions.py to update.'
       % (file_with_user_action, 1, 'NotInActionsXml')),
      output[0].message)


class PydepsNeedsUpdatingTest(unittest.TestCase):

  class MockSubprocess(object):
    CalledProcessError = subprocess.CalledProcessError

  def setUp(self):
    mock_all_pydeps = ['A.pydeps', 'B.pydeps']
    self.old_ALL_PYDEPS_FILES = PRESUBMIT._ALL_PYDEPS_FILES
    PRESUBMIT._ALL_PYDEPS_FILES = mock_all_pydeps
    self.mock_input_api = MockInputApi()
    self.mock_output_api = MockOutputApi()
    self.mock_input_api.subprocess = PydepsNeedsUpdatingTest.MockSubprocess()
    self.checker = PRESUBMIT.PydepsChecker(self.mock_input_api, mock_all_pydeps)
    self.checker._file_cache = {
        'A.pydeps': '# Generated by:\n# CMD A\nA.py\nC.py\n',
        'B.pydeps': '# Generated by:\n# CMD B\nB.py\nC.py\n',
    }

  def tearDown(self):
    PRESUBMIT._ALL_PYDEPS_FILES = self.old_ALL_PYDEPS_FILES

  def _RunCheck(self):
    return PRESUBMIT._CheckPydepsNeedsUpdating(self.mock_input_api,
                                               self.mock_output_api,
                                               checker_for_tests=self.checker)

  def testAddedPydep(self):
    # PRESUBMIT._CheckPydepsNeedsUpdating is only implemented for Android.
    if self.mock_input_api.platform != 'linux2':
      return []

    self.mock_input_api.files = [
      MockAffectedFile('new.pydeps', [], action='A'),
    ]

    results = self._RunCheck()
    self.assertEqual(1, len(results))
    self.assertTrue('PYDEPS_FILES' in str(results[0]))

  def testRemovedPydep(self):
    # PRESUBMIT._CheckPydepsNeedsUpdating is only implemented for Android.
    if self.mock_input_api.platform != 'linux2':
      return []

    self.mock_input_api.files = [
      MockAffectedFile(PRESUBMIT._ALL_PYDEPS_FILES[0], [], action='D'),
    ]

    results = self._RunCheck()
    self.assertEqual(1, len(results))
    self.assertTrue('PYDEPS_FILES' in str(results[0]))

  def testRandomPyIgnored(self):
    # PRESUBMIT._CheckPydepsNeedsUpdating is only implemented for Android.
    if self.mock_input_api.platform != 'linux2':
      return []

    self.mock_input_api.files = [
      MockAffectedFile('random.py', []),
    ]

    results = self._RunCheck()
    self.assertEqual(0, len(results), 'Unexpected results: %r' % results)

  def testRelevantPyNoChange(self):
    # PRESUBMIT._CheckPydepsNeedsUpdating is only implemented for Android.
    if self.mock_input_api.platform != 'linux2':
      return []

    self.mock_input_api.files = [
      MockAffectedFile('A.py', []),
    ]

    def mock_check_output(cmd, shell=False):
      self.assertEqual('CMD A --output ""', cmd)
      return self.checker._file_cache['A.pydeps']

    self.mock_input_api.subprocess.check_output = mock_check_output

    results = self._RunCheck()
    self.assertEqual(0, len(results), 'Unexpected results: %r' % results)

  def testRelevantPyOneChange(self):
    # PRESUBMIT._CheckPydepsNeedsUpdating is only implemented for Android.
    if self.mock_input_api.platform != 'linux2':
      return []

    self.mock_input_api.files = [
      MockAffectedFile('A.py', []),
    ]

    def mock_check_output(cmd, shell=False):
      self.assertEqual('CMD A --output ""', cmd)
      return 'changed data'

    self.mock_input_api.subprocess.check_output = mock_check_output

    results = self._RunCheck()
    self.assertEqual(1, len(results))
    self.assertTrue('File is stale' in str(results[0]))

  def testRelevantPyTwoChanges(self):
    # PRESUBMIT._CheckPydepsNeedsUpdating is only implemented for Android.
    if self.mock_input_api.platform != 'linux2':
      return []

    self.mock_input_api.files = [
      MockAffectedFile('C.py', []),
    ]

    def mock_check_output(cmd, shell=False):
      return 'changed data'

    self.mock_input_api.subprocess.check_output = mock_check_output

    results = self._RunCheck()
    self.assertEqual(2, len(results))
    self.assertTrue('File is stale' in str(results[0]))
    self.assertTrue('File is stale' in str(results[1]))


class AndroidDeprecatedTestAnnotationTest(unittest.TestCase):
  def testCheckAndroidTestAnnotationUsage(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()

    mock_input_api.files = [
        MockAffectedFile('LalaLand.java', [
          'random stuff'
        ]),
        MockAffectedFile('CorrectUsage.java', [
          'import android.support.test.filters.LargeTest;',
          'import android.support.test.filters.MediumTest;',
          'import android.support.test.filters.SmallTest;',
        ]),
        MockAffectedFile('UsedDeprecatedLargeTestAnnotation.java', [
          'import android.test.suitebuilder.annotation.LargeTest;',
        ]),
        MockAffectedFile('UsedDeprecatedMediumTestAnnotation.java', [
          'import android.test.suitebuilder.annotation.MediumTest;',
        ]),
        MockAffectedFile('UsedDeprecatedSmallTestAnnotation.java', [
          'import android.test.suitebuilder.annotation.SmallTest;',
        ]),
        MockAffectedFile('UsedDeprecatedSmokeAnnotation.java', [
          'import android.test.suitebuilder.annotation.Smoke;',
        ])
    ]
    msgs = PRESUBMIT._CheckAndroidTestAnnotationUsage(
        mock_input_api, mock_output_api)
    self.assertEqual(1, len(msgs),
                     'Expected %d items, found %d: %s'
                     % (1, len(msgs), msgs))
    self.assertEqual(4, len(msgs[0].items),
                     'Expected %d items, found %d: %s'
                     % (4, len(msgs[0].items), msgs[0].items))
    self.assertTrue('UsedDeprecatedLargeTestAnnotation.java:1' in msgs[0].items,
                    'UsedDeprecatedLargeTestAnnotation not found in errors')
    self.assertTrue('UsedDeprecatedMediumTestAnnotation.java:1'
                    in msgs[0].items,
                    'UsedDeprecatedMediumTestAnnotation not found in errors')
    self.assertTrue('UsedDeprecatedSmallTestAnnotation.java:1' in msgs[0].items,
                    'UsedDeprecatedSmallTestAnnotation not found in errors')
    self.assertTrue('UsedDeprecatedSmokeAnnotation.java:1' in msgs[0].items,
                    'UsedDeprecatedSmokeAnnotation not found in errors')



class LogUsageTest(unittest.TestCase):

  def testCheckAndroidCrLogUsage(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()

    mock_input_api.files = [
      MockAffectedFile('RandomStuff.java', [
        'random stuff'
      ]),
      MockAffectedFile('HasAndroidLog.java', [
        'import android.util.Log;',
        'some random stuff',
        'Log.d("TAG", "foo");',
      ]),
      MockAffectedFile('HasExplicitUtilLog.java', [
        'some random stuff',
        'android.util.Log.d("TAG", "foo");',
      ]),
      MockAffectedFile('IsInBasePackage.java', [
        'package org.chromium.base;',
        'private static final String TAG = "cr_Foo";',
        'Log.d(TAG, "foo");',
      ]),
      MockAffectedFile('IsInBasePackageButImportsLog.java', [
        'package org.chromium.base;',
        'import android.util.Log;',
        'private static final String TAG = "cr_Foo";',
        'Log.d(TAG, "foo");',
      ]),
      MockAffectedFile('HasBothLog.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'private static final String TAG = "cr_Foo";',
        'Log.d(TAG, "foo");',
        'android.util.Log.d("TAG", "foo");',
      ]),
      MockAffectedFile('HasCorrectTag.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'private static final String TAG = "cr_Foo";',
        'Log.d(TAG, "foo");',
      ]),
      MockAffectedFile('HasOldTag.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'private static final String TAG = "cr.Foo";',
        'Log.d(TAG, "foo");',
      ]),
      MockAffectedFile('HasDottedTag.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'private static final String TAG = "cr_foo.bar";',
        'Log.d(TAG, "foo");',
      ]),
      MockAffectedFile('HasNoTagDecl.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'Log.d(TAG, "foo");',
      ]),
      MockAffectedFile('HasIncorrectTagDecl.java', [
        'import org.chromium.base.Log;',
        'private static final String TAHG = "cr_Foo";',
        'some random stuff',
        'Log.d(TAG, "foo");',
      ]),
      MockAffectedFile('HasInlineTag.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'private static final String TAG = "cr_Foo";',
        'Log.d("TAG", "foo");',
      ]),
      MockAffectedFile('HasUnprefixedTag.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'private static final String TAG = "rubbish";',
        'Log.d(TAG, "foo");',
      ]),
      MockAffectedFile('HasTooLongTag.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'private static final String TAG = "21_charachers_long___";',
        'Log.d(TAG, "foo");',
      ]),
    ]

    msgs = PRESUBMIT._CheckAndroidCrLogUsage(
        mock_input_api, mock_output_api)

    self.assertEqual(5, len(msgs),
                     'Expected %d items, found %d: %s' % (5, len(msgs), msgs))

    # Declaration format
    nb = len(msgs[0].items)
    self.assertEqual(2, nb,
                     'Expected %d items, found %d: %s' % (2, nb, msgs[0].items))
    self.assertTrue('HasNoTagDecl.java' in msgs[0].items)
    self.assertTrue('HasIncorrectTagDecl.java' in msgs[0].items)

    # Tag length
    nb = len(msgs[1].items)
    self.assertEqual(1, nb,
                     'Expected %d items, found %d: %s' % (1, nb, msgs[1].items))
    self.assertTrue('HasTooLongTag.java' in msgs[1].items)

    # Tag must be a variable named TAG
    nb = len(msgs[2].items)
    self.assertEqual(1, nb,
                     'Expected %d items, found %d: %s' % (1, nb, msgs[2].items))
    self.assertTrue('HasInlineTag.java:4' in msgs[2].items)

    # Util Log usage
    nb = len(msgs[3].items)
    self.assertEqual(2, nb,
                     'Expected %d items, found %d: %s' % (2, nb, msgs[3].items))
    self.assertTrue('HasAndroidLog.java:3' in msgs[3].items)
    self.assertTrue('IsInBasePackageButImportsLog.java:4' in msgs[3].items)

    # Tag must not contain
    nb = len(msgs[4].items)
    self.assertEqual(2, nb,
                     'Expected %d items, found %d: %s' % (2, nb, msgs[4].items))
    self.assertTrue('HasDottedTag.java' in msgs[4].items)
    self.assertTrue('HasOldTag.java' in msgs[4].items)

class GoogleAnswerUrlFormatTest(unittest.TestCase):

  def testCatchAnswerUrlId(self):
    input_api = MockInputApi()
    input_api.files = [
      MockFile('somewhere/file.cc',
               ['char* host = '
                '  "https://support.google.com/chrome/answer/123456";']),
      MockFile('somewhere_else/file.cc',
               ['char* host = '
                '  "https://support.google.com/chrome/a/answer/123456";']),
    ]

    warnings = PRESUBMIT._CheckGoogleSupportAnswerUrl(
      input_api, MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(2, len(warnings[0].items))

  def testAllowAnswerUrlParam(self):
    input_api = MockInputApi()
    input_api.files = [
      MockFile('somewhere/file.cc',
               ['char* host = '
                '  "https://support.google.com/chrome/?p=cpn_crash_reports";']),
    ]

    warnings = PRESUBMIT._CheckGoogleSupportAnswerUrl(
      input_api, MockOutputApi())
    self.assertEqual(0, len(warnings))

class HardcodedGoogleHostsTest(unittest.TestCase):

  def testWarnOnAssignedLiterals(self):
    input_api = MockInputApi()
    input_api.files = [
      MockFile('content/file.cc',
               ['char* host = "https://www.google.com";']),
      MockFile('content/file.cc',
               ['char* host = "https://www.googleapis.com";']),
      MockFile('content/file.cc',
               ['char* host = "https://clients1.google.com";']),
    ]

    warnings = PRESUBMIT._CheckHardcodedGoogleHostsInLowerLayers(
      input_api, MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(3, len(warnings[0].items))

  def testAllowInComment(self):
    input_api = MockInputApi()
    input_api.files = [
      MockFile('content/file.cc',
               ['char* host = "https://www.aol.com"; // google.com'])
    ]

    warnings = PRESUBMIT._CheckHardcodedGoogleHostsInLowerLayers(
      input_api, MockOutputApi())
    self.assertEqual(0, len(warnings))


class ForwardDeclarationTest(unittest.TestCase):
  def testCheckHeadersOnlyOutsideThirdParty(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('somewhere/file.cc', [
        'class DummyClass;'
      ]),
      MockAffectedFile('third_party/header.h', [
        'class DummyClass;'
      ])
    ]
    warnings = PRESUBMIT._CheckUselessForwardDeclarations(mock_input_api,
      MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testNoNestedDeclaration(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('somewhere/header.h', [
        'class SomeClass {',
        ' protected:',
        '  class NotAMatch;',
        '};'
      ])
    ]
    warnings = PRESUBMIT._CheckUselessForwardDeclarations(mock_input_api,
      MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testSubStrings(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('somewhere/header.h', [
        'class NotUsefulClass;',
        'struct SomeStruct;',
        'UsefulClass *p1;',
        'SomeStructPtr *p2;'
      ])
    ]
    warnings = PRESUBMIT._CheckUselessForwardDeclarations(mock_input_api,
      MockOutputApi())
    self.assertEqual(2, len(warnings))

  def testUselessForwardDeclaration(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('somewhere/header.h', [
        'class DummyClass;',
        'struct DummyStruct;',
        'class UsefulClass;',
        'std::unique_ptr<UsefulClass> p;'
      ])
    ]
    warnings = PRESUBMIT._CheckUselessForwardDeclarations(mock_input_api,
      MockOutputApi())
    self.assertEqual(2, len(warnings))

  def testBlinkHeaders(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('third_party/WebKit/header.h', [
        'class DummyClass;',
        'struct DummyStruct;',
      ]),
      MockAffectedFile('third_party\\WebKit\\header.h', [
        'class DummyClass;',
        'struct DummyStruct;',
      ])
    ]
    warnings = PRESUBMIT._CheckUselessForwardDeclarations(mock_input_api,
      MockOutputApi())
    self.assertEqual(4, len(warnings))


class RiskyJsTest(unittest.TestCase):
  def testArrowWarnInIos9Code(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()

    mock_input_api.files = [
      MockAffectedFile('components/blah.js', ["shouldn't use => here"]),
    ]
    warnings = PRESUBMIT._CheckForRiskyJsFeatures(
        mock_input_api, mock_output_api)
    self.assertEqual(1, len(warnings))

    mock_input_api.files = [
      MockAffectedFile('ios/blee.js', ['might => break folks']),
    ]
    warnings = PRESUBMIT._CheckForRiskyJsFeatures(
        mock_input_api, mock_output_api)
    self.assertEqual(1, len(warnings))

    mock_input_api.files = [
      MockAffectedFile('ui/webui/resources/blarg.js', ['on => iOS9']),
    ]
    warnings = PRESUBMIT._CheckForRiskyJsFeatures(
        mock_input_api, mock_output_api)
    self.assertEqual(1, len(warnings))

  def testArrowsAllowedInChromeCode(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('chrome/browser/resources/blah.js', 'arrow => OK here'),
    ]
    warnings = PRESUBMIT._CheckForRiskyJsFeatures(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(warnings))

class RelativeIncludesTest(unittest.TestCase):
  def testThirdPartyNotWebKitIgnored(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('third_party/test.cpp', '#include "../header.h"'),
      MockAffectedFile('third_party/test/test.cpp', '#include "../header.h"'),
    ]

    mock_output_api = MockOutputApi()

    errors = PRESUBMIT._CheckForRelativeIncludes(
        mock_input_api, mock_output_api)
    self.assertEqual(0, len(errors))

  def testNonCppFileIgnored(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('test.py', '#include "../header.h"'),
    ]

    mock_output_api = MockOutputApi()

    errors = PRESUBMIT._CheckForRelativeIncludes(
        mock_input_api, mock_output_api)
    self.assertEqual(0, len(errors))

  def testInnocuousChangesAllowed(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('test.cpp', '#include "header.h"'),
      MockAffectedFile('test2.cpp', '../'),
    ]

    mock_output_api = MockOutputApi()

    errors = PRESUBMIT._CheckForRelativeIncludes(
        mock_input_api, mock_output_api)
    self.assertEqual(0, len(errors))

  def testRelativeIncludeNonWebKitProducesError(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('test.cpp', ['#include "../header.h"']),
    ]

    mock_output_api = MockOutputApi()

    errors = PRESUBMIT._CheckForRelativeIncludes(
        mock_input_api, mock_output_api)
    self.assertEqual(1, len(errors))

  def testRelativeIncludeWebKitProducesError(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockAffectedFile('third_party/WebKit/test.cpp',
                       ['#include "../header.h']),
    ]

    mock_output_api = MockOutputApi()

    errors = PRESUBMIT._CheckForRelativeIncludes(
        mock_input_api, mock_output_api)
    self.assertEqual(1, len(errors))

if __name__ == '__main__':
  unittest.main()
