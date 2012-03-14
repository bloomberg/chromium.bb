# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path  # for initializing constants

# Directories that we run presubmit checks on.
PRESUBMIT_PATH = os.path.normpath('chrome/common/extensions/PRESUBMIT.py')
API_DIR = os.path.normpath('chrome/common/extensions/api')
DOC_DIR = os.path.normpath('chrome/common/extensions/docs')
BUILD_DIR = os.path.join(DOC_DIR, 'build')
TEMPLATE_DIR = os.path.join(DOC_DIR, 'template')
JS_DIR = os.path.join(DOC_DIR, 'js')
CSS_DIR = os.path.join(DOC_DIR, 'css')
STATIC_DIR = os.path.join(DOC_DIR, 'static')
SAMPLES_DIR = os.path.join(DOC_DIR, 'examples')

EXCEPTIONS = ['README', 'README.txt', 'OWNERS']

# Presubmit messages.
README = os.path.join(DOC_DIR, 'README.txt')
REBUILD_WARNING = (
    'This change modifies the extension docs but the generated docs have '
    'not been updated properly. See %s for more info.' % README)
BUILD_SCRIPT = os.path.join(BUILD_DIR, 'build.py')
REBUILD_INSTRUCTIONS = (
    'First build DumpRenderTree, then update the docs by running:\n  %s' %
    BUILD_SCRIPT)


def CheckChangeOnUpload(input_api, output_api):
  return (CheckPresubmitChanges(input_api, output_api) +
          CheckDocChanges(input_api, output_api))

def CheckChangeOnCommit(input_api, output_api):
  return (CheckPresubmitChanges(input_api, output_api) +
          CheckDocChanges(input_api, output_api, strict=False))

def CheckPresubmitChanges(input_api, output_api):
  for PRESUBMIT_PATH in input_api.LocalPaths():
    return input_api.canned_checks.RunUnitTests(input_api, output_api,
                                                ['./PRESUBMIT_test.py'])
  return []

def CheckDocChanges(input_api, output_api, strict=True):
  warnings = []

  for af in input_api.AffectedFiles():
    path = af.LocalPath()
    if IsSkippedFile(path, input_api):
      continue

    elif (IsApiFile(path, input_api) or
          IsBuildFile(path, input_api) or
          IsTemplateFile(path, input_api) or
          IsJsFile(path, input_api) or
          IsCssFile(path, input_api)):
      # These files do not always cause changes to the generated docs
      # so we can ignore them if not running strict checks.
      if strict and not DocsGenerated(input_api):
        warnings.append('Docs out of sync with %s changes.' % path)

    elif IsStaticDoc(path, input_api):
      if not StaticDocBuilt(af, input_api):
        warnings.append('Changes to %s not reflected in generated doc.' % path)

    elif IsSampleFile(path, input_api):
      if not SampleZipped(af, input_api):
        warnings.append('Changes to sample %s have not been re-zipped.' % path)

    elif IsGeneratedDoc(path, input_api):
      if not NonGeneratedFilesEdited(input_api):
        warnings.append('Changes to generated doc %s not reflected in '
                        'non-generated files.' % path)

  if warnings:
    warnings.sort()
    warnings = [' - %s\n' % w for w in warnings]
    # Prompt user if they want to continue.
    return [output_api.PresubmitPromptWarning(REBUILD_WARNING + '\n' +
                                              ''.join(warnings) +
                                              REBUILD_INSTRUCTIONS)]
  return []

def IsSkippedFile(path, input_api):
  return input_api.os_path.basename(path) in EXCEPTIONS

def IsApiFile(path, input_api):
  return (input_api.os_path.dirname(path) == API_DIR and
          path.endswith('.json'))

def IsBuildFile(path, input_api):
  return input_api.os_path.dirname(path) == BUILD_DIR

def IsTemplateFile(path, input_api):
  return input_api.os_path.dirname(path) == TEMPLATE_DIR

def IsJsFile(path, input_api):
  return (input_api.os_path.dirname(path) == JS_DIR and
          path.endswith('.js'))

def IsCssFile(path, input_api):
  return (input_api.os_path.dirname(path) == CSS_DIR and
          path.endswith('.css'))

def IsStaticDoc(path, input_api):
  return (input_api.os_path.dirname(path) == STATIC_DIR and
          path.endswith('.html'))

def IsSampleFile(path, input_api):
  return input_api.os_path.dirname(path).startswith(SAMPLES_DIR)

def IsGeneratedDoc(path, input_api):
  return (input_api.os_path.dirname(path) == DOC_DIR and
          path.endswith('.html'))

def DocsGenerated(input_api):
  """Return True if there are any generated docs in this change.

  Generated docs are the files that are the output of build.py. Typically
  all docs changes will contain both generated docs and non-generated files.
  """
  return any(IsGeneratedDoc(path, input_api)
             for path in input_api.LocalPaths())

def NonGeneratedFilesEdited(input_api):
  """Return True if there are any non-generated files in this change.

  Non-generated files are those that are the input to build.py. Typically
  all docs changes will contain both non-generated files and generated docs.
  """
  return any(IsApiFile(path, input_api) or
             IsBuildFile(path, input_api) or
             IsTemplateFile(path, input_api) or
             IsJsFile(path, input_api) or
             IsCssFile(path, input_api) or
             IsStaticDoc(path, input_api) or
             IsSampleFile(path, input_api)
             for path in input_api.LocalPaths())

def StaticDocBuilt(static_file, input_api):
  """Return True if the generated doc that corresponds to the |static_file|
  is also in this change. Both files must also contain matching changes.
  """
  generated_file = _FindFileInAlternateDir(static_file, DOC_DIR, input_api)
  return _ChangesMatch(generated_file, static_file)

def _FindFileInAlternateDir(affected_file, alt_dir, input_api):
  """Return an AffectFile for the file in |alt_dir| that corresponds to
  |affected_file|.

  If the file does not exist in the is change, return None.
  """
  alt_path = _AlternateFilePath(affected_file.LocalPath(), alt_dir, input_api)
  for f in input_api.AffectedFiles():
    if f.LocalPath() == alt_path:
      return f

def _AlternateFilePath(path, alt_dir, input_api):
  """Return a path with the same basename as |path| but in |alt_dir| directory.

  This is useful for finding corresponding static and generated docs.

  Example:
    _AlternateFilePath('/foo/bar', '/alt/dir', ...) == '/alt/dir/bar')
  """
  base_name = input_api.os_path.basename(path)
  return input_api.os_path.join(alt_dir, base_name)

def _ChangesMatch(generated_file, static_file):
  """Return True if the two files contain the same textual changes.

  There may be extra generated lines and generated lines are still considered
  to "match" static ones even if they have extra formatting/text at their
  beginnings and ends.
  Line numbers may differ but order may not.
  """
  if not generated_file and not static_file:
    return True  # Neither file affected.

  if not generated_file or not static_file:
    return False  # One file missing.

  generated_changes = generated_file.ChangedContents()
  static_changes = static_file.ChangedContents()
  # ChangedContents() is a list of (line number, text) for all new lines.
  # Ignore the line number, but check that the text for each new line matches.

  next_generated = 0
  start_pos = 0
  for next_static in range(len(static_changes)):
    _, static_text = static_changes[next_static]

    # Search generated changes for this static text.
    found = False
    while not found and next_generated < len(generated_changes):
      _, generated_text = generated_changes[next_generated]
      # Text need not be identical but the entire static line should be
      # in the generated one (e.g. generated text might have extra formatting).
      found_at = generated_text[start_pos:].find(static_text)
      if found_at != -1:
        # Next search starts on the same line, after the substring matched.
        start_pos = found_at + len(static_text)
        found  = True
      else:
        next_generated += 1
        start_pos = 0

    if not found:
      return False

  return True

def SampleZipped(sample_file, input_api):
  """Return True if the zipfile that should contain |sample_file| is in
  this change.
  """
  sample_path = sample_file.LocalPath()
  for af in input_api.AffectedFiles():
    root, ext = input_api.os_path.splitext(af.LocalPath())
    if ext == '.zip' and sample_path.startswith(root):
      return True
  return False
