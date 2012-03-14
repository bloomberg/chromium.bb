#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import os.path
import PRESUBMIT


class FakeInputApi(object):
  os_path = os.path
  def __init__(self, affected_files=[]):
    self.affected_files = affected_files
  def AffectedFiles(self):
    return self.affected_files
  def LocalPaths(self):
    return [af.LocalPath() for af in self.AffectedFiles()]


class FakeAffectedFile(object):
  def __init__(self, local_path='', changed_contents=None):
    self._local_path = os.path.normpath(local_path)
    self._changed_contents = changed_contents
  def LocalPath(self):
    return self._local_path
  def ChangedContents(self):
    return self._changed_contents


class FakeOutputApi(object):
  def PresubmitPromptWarning(self, warning_msg):
    return warning_msg


class TestPresubmit(unittest.TestCase):
  def setUp(self):
    self.input_api = FakeInputApi()
    self.output_api = FakeOutputApi()

  def assertPathsEqual(self, path1, path2):
    self.assertEqual(os.path.normpath(path1), os.path.normpath(path2))

  def testCheckDocsChanges_NoFiles(self):
    input_api = FakeInputApi(affected_files=[])
    self.assertEqual([], PRESUBMIT.CheckDocChanges(input_api, self.output_api))

  def testCheckDocsChanges_OnlyExcpetions(self):
    input_api = FakeInputApi(affected_files=[
        FakeAffectedFile(local_path='chrome/common/extensions/docs/README'),
        FakeAffectedFile(local_path='chrome/common/extensions/docs/OWNERS')])
    self.assertEqual([], PRESUBMIT.CheckDocChanges(input_api, self.output_api))

  def testCheckDocsChanges_NoGeneratedFiles(self):
    input_api = FakeInputApi(affected_files=[
        FakeAffectedFile(
            local_path='chrome/common/extensions/api/foo.json'),
        FakeAffectedFile(
            local_path='chrome/common/extensions/docs/examples/foo/b/bz.js'),
        FakeAffectedFile(
            local_path='chrome/common/extensions/docs/static/foo.html')])
    expected_warning = (
        'This change modifies the extension docs but the generated docs '
        'have not been updated properly. See %s for more info.\n'
        ' - Changes to %s not reflected in generated doc.\n'
        ' - Changes to sample %s have not been re-zipped.\n'
        ' - Docs out of sync with %s changes.\n'
        'First build DumpRenderTree, then update the docs by running:\n  %s' %
        (os.path.normpath('chrome/common/extensions/docs/README.txt'),
         os.path.normpath('chrome/common/extensions/docs/static/foo.html'),
         os.path.normpath('chrome/common/extensions/docs/examples/foo/b/bz.js'),
         os.path.normpath('chrome/common/extensions/api/foo.json'),
         os.path.normpath('chrome/common/extensions/docs/build/build.py')))
    self.assertEqual([expected_warning],
                     PRESUBMIT.CheckDocChanges(input_api, self.output_api))

  def testCheckDocsChanges_OnlyGeneratedDocs(self):
    input_api = FakeInputApi(affected_files=[
        FakeAffectedFile(local_path='chrome/common/extensions/docs/foo.html'),
        FakeAffectedFile(local_path='chrome/common/extensions/docs/bar.html')])
    expected_warning = (
        'This change modifies the extension docs but the generated docs '
        'have not been updated properly. See %s for more info.\n'
        ' - Changes to generated doc %s not reflected in non-generated files.\n'
        ' - Changes to generated doc %s not reflected in non-generated files.\n'
        'First build DumpRenderTree, then update the docs by running:\n  %s' %
        (os.path.normpath('chrome/common/extensions/docs/README.txt'),
         os.path.normpath('chrome/common/extensions/docs/bar.html'),
         os.path.normpath('chrome/common/extensions/docs/foo.html'),
         os.path.normpath('chrome/common/extensions/docs/build/build.py')))
    self.assertEqual([expected_warning],
                     PRESUBMIT.CheckDocChanges(input_api, self.output_api))

  def testIsSkippedFile(self):
    self.assertTrue(PRESUBMIT.IsSkippedFile(
        os.path.normpath('chrome/common/extensions/docs/README.txt'),
        self.input_api))
    self.assertTrue(PRESUBMIT.IsSkippedFile(
        os.path.normpath('chrome/common/extensions/OWNERS'), self.input_api))
    self.assertTrue(PRESUBMIT.IsSkippedFile(
        os.path.normpath('chrome/common/extensions/api/README'),
        self.input_api))
    self.assertTrue(PRESUBMIT.IsSkippedFile(
        os.path.normpath('foo/bar/README.txt'), self.input_api))
    self.assertFalse(PRESUBMIT.IsSkippedFile(
        os.path.normpath('chrome/common/extensions/README/foo'),
        self.input_api))

  def testIsApiFile(self):
    self.assertFalse(PRESUBMIT.IsApiFile(
        os.path.normpath('chrome/common/extensions/docs/foo.html'),
        self.input_api))
    self.assertTrue(PRESUBMIT.IsApiFile(
        os.path.normpath('chrome/common/extensions/api/foo.json'),
        self.input_api))
    self.assertFalse(PRESUBMIT.IsApiFile(
        os.path.normpath('chrome/common/extensions/api/foo.html'),
        self.input_api))
    self.assertFalse(PRESUBMIT.IsApiFile(
        os.path.normpath('chrome/common/extensions/docs/api/foo.html'),
        self.input_api))

  def testIsBuildFile(self):
    self.assertTrue(PRESUBMIT.IsBuildFile(
        os.path.normpath('chrome/common/extensions/docs/build/foo.py'),
        self.input_api))
    self.assertTrue(PRESUBMIT.IsBuildFile(
        os.path.normpath('chrome/common/extensions/docs/build/bar.html'),
        self.input_api))
    self.assertFalse(PRESUBMIT.IsBuildFile(
        os.path.normpath('chrome/common/extensions/docs/build.py'),
        self.input_api))
    self.assertFalse(PRESUBMIT.IsBuildFile(
        os.path.normpath('chrome/common/extensions/build/foo.py'),
        self.input_api))

  def testIsTemplateFile(self):
    self.assertTrue(PRESUBMIT.IsTemplateFile(
        os.path.normpath('chrome/common/extensions/docs/template/foo.html'),
        self.input_api))
    self.assertTrue(PRESUBMIT.IsTemplateFile(
        os.path.normpath('chrome/common/extensions/docs/template/bar.js'),
        self.input_api))
    self.assertFalse(PRESUBMIT.IsTemplateFile(
        os.path.normpath('chrome/common/extensions/docs/template.html'),
        self.input_api))
    self.assertFalse(PRESUBMIT.IsTemplateFile(
        os.path.normpath('chrome/common/extensions/template/foo.html'),
        self.input_api))

  def testIsStaticDoc(self):
    self.assertFalse(PRESUBMIT.IsStaticDoc(
        os.path.normpath('chrome/common/extensions/docs/foo.html'),
        self.input_api))
    self.assertTrue(PRESUBMIT.IsStaticDoc(
        os.path.normpath('chrome/common/extensions/docs/static/foo.html'),
        self.input_api))
    self.assertFalse(PRESUBMIT.IsStaticDoc(
        os.path.normpath('chrome/common/extensions/docs/api/foo.html'),
        self.input_api))
    self.assertFalse(PRESUBMIT.IsStaticDoc(
        os.path.normpath('chrome/common/extensions/docs/static/foo.css'),
        self.input_api))

  def testIsSampleFile(self):
    self.assertTrue(PRESUBMIT.IsSampleFile(
        os.path.normpath(
            'chrome/common/extensions/docs/examples/ex1/manifest.json'),
        self.input_api))
    self.assertTrue(PRESUBMIT.IsSampleFile(
        os.path.normpath(
            'chrome/common/extensions/docs/examples/ex1/images/icon.png'),
        self.input_api))
    self.assertTrue(PRESUBMIT.IsSampleFile(
        os.path.normpath('chrome/common/extensions/docs/examples/dir'),
        self.input_api))
    self.assertTrue(PRESUBMIT.IsSampleFile(
        os.path.normpath('chrome/common/extensions/docs/examples/dir/'),
        self.input_api))
    self.assertFalse(PRESUBMIT.IsSampleFile(
        os.path.normpath(
            'chrome/common/extensions/docs/samples/ex1/manifest.json'),
        self.input_api))

  def testIsJsFile(self):
    self.assertFalse(PRESUBMIT.IsJsFile(
        os.path.normpath('chrome/common/extensions/docs/foo.js'),
        self.input_api))
    self.assertTrue(PRESUBMIT.IsJsFile(
        os.path.normpath('chrome/common/extensions/docs/js/foo.js'),
        self.input_api))
    self.assertFalse(PRESUBMIT.IsJsFile(
        os.path.normpath('chrome/common/extensions/docs/js/foo.html'),
        self.input_api))

  def testIsCssFile(self):
    self.assertFalse(PRESUBMIT.IsCssFile(
        os.path.normpath('chrome/common/extensions/docs/foo.css'),
        self.input_api))
    self.assertTrue(PRESUBMIT.IsCssFile(
        os.path.normpath('chrome/common/extensions/docs/css/foo.css'),
        self.input_api))
    self.assertFalse(PRESUBMIT.IsCssFile(
        os.path.normpath('chrome/common/extensions/docs/css/foo.html'),
        self.input_api))

  def testIsGeneratedDoc(self):
    self.assertTrue(PRESUBMIT.IsGeneratedDoc(
        os.path.normpath('chrome/common/extensions/docs/foo.html'),
        self.input_api))
    self.assertFalse(PRESUBMIT.IsGeneratedDoc(
        os.path.normpath('chrome/common/extensions/docs/static/foo.html'),
        self.input_api))
    self.assertFalse(PRESUBMIT.IsGeneratedDoc(
        os.path.normpath('chrome/common/extensions/docs/api/foo.html'),
        self.input_api))
    self.assertFalse(PRESUBMIT.IsGeneratedDoc(
        os.path.normpath('chrome/common/extensions/docs/foo.css'),
        self.input_api))

  def testDocsGenerated_SomeGeneratedDocs(self):
    api_file = FakeAffectedFile(
        local_path='chrome/common/extensions/api/foo.json')
    input_api = FakeInputApi(affected_files=[
        FakeAffectedFile(local_path='chrome/common/extensions/docs/foo.html'),
        FakeAffectedFile(local_path='chrome/common/extensions/docs/bar.html')])
    self.assertTrue(PRESUBMIT.DocsGenerated(input_api))

  def testDocsGenerated_NoGeneratedDocs(self):
    api_file = FakeAffectedFile(
        local_path='chrome/common/extensions/api/foo.json')
    input_api = FakeInputApi(affected_files=[api_file])
    self.assertFalse(PRESUBMIT.DocsGenerated(input_api))

  def testNonGeneratedFilesEdited_SomeNonGeneratedFiles(self):
    input_api = FakeInputApi(affected_files=[
        FakeAffectedFile(local_path='chrome/common/extensions/api/foo.json'),
        FakeAffectedFile(
            local_path='chrome/common/extensions/docs/static/index.html')])
    self.assertTrue(PRESUBMIT.NonGeneratedFilesEdited(input_api))

  def testNonGeneratedFilesEdited_ZeroNonGeneratedFiles(self):
    input_api = FakeInputApi(affected_files=[
        FakeAffectedFile(local_path='chrome/common/extensions/docs/one.html'),
        FakeAffectedFile(local_path='chrome/common/extensions/docs/two.html')])
    self.assertFalse(PRESUBMIT.NonGeneratedFilesEdited(input_api))

  def testStaticDocBuilt_ChangesMatch(self):
    static_file = FakeAffectedFile(
        local_path='chrome/common/extensions/docs/static/index.html',
        changed_contents=[(3, 'foo!'), (4, 'bar!')])
    generated_file = FakeAffectedFile(
        local_path='chrome/common/extensions/docs/index.html',
        changed_contents=[(13, 'foo!'), (14, 'bar!')])
    input_api = FakeInputApi(affected_files=[generated_file, static_file])

    self.assertTrue(PRESUBMIT.StaticDocBuilt(static_file, input_api))

  def testStaticDocBuilt_OnlyStaticChanged(self):
    static_file = FakeAffectedFile(
        local_path='chrome/common/extensions/docs/static/index.html',
        changed_contents=[(3, 'foo!'), (4, 'bar!')])
    other_static_file = FakeAffectedFile(
        local_path='chrome/common/extensions/docs/static/next.html',
        changed_contents=[(3, 'double foo!'), (4, 'double bar!')])
    input_api = FakeInputApi(affected_files=[static_file, other_static_file])

    self.assertFalse(PRESUBMIT.StaticDocBuilt(static_file, input_api))
    self.assertFalse(PRESUBMIT.StaticDocBuilt(other_static_file, input_api))

  def testStaticDocBuilt_ChangesDoNotMatch(self):
    static_file = FakeAffectedFile(
        local_path='chrome/common/extensions/docs/static/index.html',
        changed_contents=[(3, 'double foo!'), (4, 'double bar!')])
    generated_file = FakeAffectedFile(
        local_path='chrome/common/extensions/docs/index.html',
        changed_contents=[(13, 'foo!'), (14, 'bar!')])
    input_api = FakeInputApi(affected_files=[generated_file, static_file])

    self.assertFalse(PRESUBMIT.StaticDocBuilt(static_file, input_api))

  def testAlternateFilePath(self):
    path = os.path.normpath('foo/bar/baz.html')
    alt_dir = os.path.normpath('woop/dee/doo')
    self.assertPathsEqual('woop/dee/doo/baz.html',
                          PRESUBMIT._AlternateFilePath(path, alt_dir,
                                                       self.input_api))
    alt_dir = 'foo'
    self.assertPathsEqual('foo/baz.html',
                          PRESUBMIT._AlternateFilePath(path, alt_dir,
                                                       self.input_api))
    alt_dir = ''
    self.assertPathsEqual('baz.html',
                          PRESUBMIT._AlternateFilePath(path, alt_dir,
                                                       self.input_api))

  def testFindFileInAlternateDir(self):
    input_api = FakeInputApi(affected_files=[
        FakeAffectedFile(local_path='dir/1.html'),
        FakeAffectedFile(local_path='dir/2.html'),
        FakeAffectedFile(local_path='dir/alt/1.html'),
        FakeAffectedFile(local_path='dir/alt/3.html')])
    alt_dir = os.path.normpath('dir/alt')

    # Both files in affected files.
    alt_file = PRESUBMIT._FindFileInAlternateDir(
        FakeAffectedFile(local_path='dir/1.html'), alt_dir, input_api)
    self.assertTrue(alt_file)
    self.assertPathsEqual('dir/alt/1.html', alt_file.LocalPath())

    # Given file not in affected files.
    alt_file = PRESUBMIT._FindFileInAlternateDir(
        FakeAffectedFile(local_path='dir/3.html'), alt_dir, input_api)
    self.assertTrue(alt_file)
    self.assertPathsEqual('dir/alt/3.html', alt_file.LocalPath())

    # Alternate file not found.
    alt_file = PRESUBMIT._FindFileInAlternateDir(
        FakeAffectedFile(local_path='dir/2.html'), alt_dir, input_api)
    self.assertFalse(alt_file)

  def testChangesMatch_Same(self):
    af1 = FakeAffectedFile(changed_contents=[(1, 'foo'), (14, 'bar')])
    af2 = FakeAffectedFile(changed_contents=[(1, 'foo'), (14, 'bar')])
    self.assertTrue(PRESUBMIT._ChangesMatch(af1, af2))
    self.assertTrue(PRESUBMIT._ChangesMatch(af2, af1))

  def testChangesMatch_OneFileNone(self):
    af1 = FakeAffectedFile(changed_contents=[(1, 'foo'), (14, 'bar')])
    af2 = None
    self.assertFalse(PRESUBMIT._ChangesMatch(af1, af2))
    self.assertFalse(PRESUBMIT._ChangesMatch(af2, af1))

  def testChangesMatch_BothFilesNone(self):
    af1 = None
    af2 = None
    self.assertTrue(PRESUBMIT._ChangesMatch(af1, af2))
    self.assertTrue(PRESUBMIT._ChangesMatch(af2, af1))

  def testChangesMatch_DifferentLineNumbers(self):
    af1 = FakeAffectedFile(changed_contents=[(1, 'foo'), (14, 'bar')])
    af2 = FakeAffectedFile(changed_contents=[(1, 'foo'), (15, 'bar')])
    self.assertTrue(PRESUBMIT._ChangesMatch(af1, af2))
    self.assertTrue(PRESUBMIT._ChangesMatch(af2, af1))

  def testChangesMatch_DifferentOrder(self):
    af = FakeAffectedFile(changed_contents=[(1, 'foo'), (2, 'bar')])
    af2 = FakeAffectedFile(changed_contents=[(1, 'bar'), (2, 'foo')])
    self.assertFalse(PRESUBMIT._ChangesMatch(af, af2))
    self.assertFalse(PRESUBMIT._ChangesMatch(af2, af))

  def testChangesMatch_DifferentLength(self):
    af = FakeAffectedFile(changed_contents=[(14, 'bar'), (15, 'qxxy')])
    af_extra = FakeAffectedFile(changed_contents=[(1, 'foo'), (14, 'bar'),
                                                  (23, 'baz'), (25, 'qxxy')])
    # The generated file (first arg) may have extra lines.
    self.assertTrue(PRESUBMIT._ChangesMatch(af_extra, af))
    # But not the static file (second arg).
    self.assertFalse(PRESUBMIT._ChangesMatch(af, af_extra))

  def testChangesMatch_SameLengthButDifferentText(self):
    af1 = FakeAffectedFile(changed_contents=[(1, 'foo'), (14, 'bar')])
    af2 = FakeAffectedFile(changed_contents=[(1, 'foo'), (14, 'baz')])
    self.assertFalse(PRESUBMIT._ChangesMatch(af1, af2))
    self.assertFalse(PRESUBMIT._ChangesMatch(af2, af1))

  def testChangesMatch_LineIsSubstring(self):
    af = FakeAffectedFile(changed_contents=[(1, 'foo'), (2, 'bar')])
    af_plus = FakeAffectedFile(changed_contents=[(6, 'foo'), (9, '<b>bar</b>')])
    # The generated file (first arg) can have extra formatting.
    self.assertTrue(PRESUBMIT._ChangesMatch(af_plus, af))
    # But not the static file (second arg).
    self.assertFalse(PRESUBMIT._ChangesMatch(af, af_plus))

  def testChangesMatch_DuplicateLines(self):
    af = FakeAffectedFile(changed_contents=[(1, 'foo'), (2, 'bar')])
    af_dups = FakeAffectedFile(changed_contents=[(7, 'foo'), (8, 'foo'),
                                                 (9, 'bar')])
    # Duplciate lines in the generated file (first arg) will be ignored
    # like extra lines.
    self.assertTrue(PRESUBMIT._ChangesMatch(af_dups, af))
    # Duplicate lines in static file (second arg) must each have a matching
    # line in the generated file.
    self.assertFalse(PRESUBMIT._ChangesMatch(af, af_dups))

  def testChangesMatch_SkipEmptyLinesInStaticFile(self):
    generated_file = FakeAffectedFile(changed_contents=[(1, 'foo'),
                                                        (14, 'bar')])
    static_file = FakeAffectedFile(changed_contents=[(1, 'foo'),
                                                     (2, ''),
                                                     (14, 'bar')])
    self.assertTrue(PRESUBMIT._ChangesMatch(generated_file, static_file))

  def testChangesMatch_SkipExtraTextInGeneratedFile(self):
    generated_file = FakeAffectedFile(changed_contents=[(1, 'foo'),
                                                        (14, 'bar'),
                                                        (15, 'baz')])
    static_file = FakeAffectedFile(changed_contents=[(1, 'bar')])
    self.assertTrue(PRESUBMIT._ChangesMatch(generated_file, static_file))

  def testChangesMatch_Multiline(self):
    generated_file = FakeAffectedFile(changed_contents=[(1, '<pre>{')])
    static_file = FakeAffectedFile(changed_contents=[(1, 'pre'), (2, '{')])
    self.assertTrue(PRESUBMIT._ChangesMatch(generated_file, static_file))

  def testSampleZipped_ZipInAffectedFiles(self):
    sample_file = FakeAffectedFile(
        local_path='chrome/common/extensions/docs/examples/foo/bar/baz.jpg')
    zip_file = FakeAffectedFile(
        local_path='chrome/common/extensions/docs/examples/foo/bar.zip')
    input_api = FakeInputApi(affected_files=[sample_file, zip_file])
    self.assertTrue(PRESUBMIT.SampleZipped(sample_file, input_api))

  def testSampleZipped_NoZip(self):
    sample_file = FakeAffectedFile(
        local_path='chrome/common/extensions/docs/examples/foo/bar.jpg')
    input_api = FakeInputApi(affected_files=[sample_file])

    self.assertFalse(PRESUBMIT.SampleZipped(sample_file, input_api))

  def testSampleZipped_WrongZip(self):
    sample_file = FakeAffectedFile(
        local_path='chrome/common/extensions/docs/examples/foo/bar.jpg')
    zip_file = FakeAffectedFile(
        local_path='chrome/common/extensions/docs/examples/baz.zip')
    input_api = FakeInputApi(affected_files=[sample_file, zip_file])

    self.assertFalse(PRESUBMIT.SampleZipped(sample_file, input_api))


if __name__ == '__main__':
  unittest.main()
