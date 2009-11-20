# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Chromium.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

EXCLUDED_PATHS = (
    r"breakpad[\\\/].*",
    r"skia[\\\/].*",
    r"v8[\\\/].*",
)


def CheckChangeOnUpload(input_api, output_api):
  results = []
  # What does this code do?
  # It loads the default black list (e.g. third_party, experimental, etc) and
  # add our black list (breakpad, skia and v8 are still not following
  # google style and are not really living this repository).
  # See presubmit_support.py InputApi.FilterSourceFile for the (simple) usage.
  black_list = input_api.DEFAULT_BLACK_LIST + EXCLUDED_PATHS
  sources = lambda x: input_api.FilterSourceFile(x, black_list=black_list)
  results.extend(input_api.canned_checks.CheckLongLines(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckChangeHasNoTabs(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckChangeHasNoStrayWhitespace(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckChangeHasBugField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasTestField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeSvnEolStyle(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckSvnForCommonMimeTypes(
      input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  black_list = input_api.DEFAULT_BLACK_LIST + EXCLUDED_PATHS
  sources = lambda x: input_api.FilterSourceFile(x, black_list=black_list)
  results.extend(input_api.canned_checks.CheckLongLines(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckChangeHasNoTabs(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckChangeHasNoStrayWhitespace(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckChangeHasBugField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasTestField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeSvnEolStyle(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckSvnForCommonMimeTypes(
      input_api, output_api))
  # TODO(thestig) temporarily disabled, doesn't work in third_party/
  #results.extend(input_api.canned_checks.CheckSvnModifiedDirectories(
  #    input_api, output_api, sources))
  # Make sure the tree is 'open'.
  # TODO(maruel): Run it in a separate thread to parallelize checks?
  results.extend(CheckTreeIsOpen(
      input_api,
      output_api,
      'http://chromium-status.appspot.com/status',
      '0',
      'http://chromium-status.appspot.com/current?format=raw'))
  results.extend(CheckTryJobExecution(input_api, output_api))
  # These builders are just too slow.
  IGNORED_BUILDERS = [
    'Chromium XP',
    'Chromium Mac',
    'Chromium Mac (valgrind)',
    'Chromium Mac UI (valgrind)(1)',
    'Chromium Mac UI (valgrind)(2)',
    'Chromium Mac UI (valgrind)(3)',
    'Chromium Mac (tsan)',
    'Webkit Mac (valgrind)',
    'Chromium Linux',
    'Chromium Linux x64',
    'Linux Tests (valgrind)(1)',
    'Linux Tests (valgrind)(2)',
    'Linux Tests (valgrind)(3)',
    'Linux Tests (valgrind)(4)',
    'Webkit Linux (valgrind layout)',
  ]
  results.extend(CheckPendingBuilds(
      input_api,
      output_api,
      'http://build.chromium.org/buildbot/waterfall/json/builders',
      6,
      IGNORED_BUILDERS))
  return results


def CheckTryJobExecution(input_api, output_api):
  outputs = []
  if not input_api.change.issue or not input_api.change.patchset:
    return outputs
  url = "http://codereview.chromium.org/%d/get_build_results/%d" % (
            input_api.change.issue, input_api.change.patchset)
  PLATFORMS = ('win', 'linux', 'mac')
  try:
    connection = input_api.urllib2.urlopen(url)
    # platform|status|url
    values = [item.split('|', 2) for item in connection.read().splitlines()]
    connection.close()
    if not values:
      # It returned an empty list. Probably a private review.
      return outputs
    # Reformat as an dict of platform: [status, url]
    values = dict([[v[0], [v[1], v[2]]] for v in values])
    for platform in PLATFORMS:
      values.setdefault(platform, ['not started', ''])
    message = None
    non_success = [k.upper() for k,v in values.iteritems() if v[0] != 'success']
    if 'failure' in [v[0] for v in values.itervalues()]:
      message = 'Try job failures on %s!\n' % ', '.join(non_success)
    elif non_success:
      message = ('Unfinished (or not even started) try jobs on '
                 '%s.\n') % ', '.join(non_success)
    if message:
      message += (
          'Is try server wrong or broken? Please notify maruel@chromium.org. '
          'Thanks.\n')
      outputs.append(output_api.PresubmitPromptWarning(message=message))
  except input_api.urllib2.HTTPError, e:
    if e.code == 404:
      # Fallback to no try job.
      # TODO(maruel): Change to a PresubmitPromptWarning once the try server is
      # stable enough and it seems to work fine.
      outputs.append(output_api.PresubmitNotifyResult(
          'You should try the patch first.'))
    else:
      # Another HTTP error happened, warn the user.
      # TODO(maruel): Change to a PresubmitPromptWarning once it deemed to work
      # fine.
      outputs.append(output_api.PresubmitNotifyResult(
          'Got %s while looking for try job status.' % str(e)))
  return outputs


def CheckTreeIsOpen(input_api, output_api, url, closed, url_text):
  """Similar to the one in presubmit_canned_checks except it shows an helpful
  status text instead.
  """
  assert(input_api.is_committing)
  try:
    connection = input_api.urllib2.urlopen(url)
    status = connection.read()
    connection.close()
    if input_api.re.match(closed, status):
      long_text = status + '\n' + url
      try:
        connection = input_api.urllib2.urlopen(url_text)
        long_text = connection.read().strip()
        connection.close()
      except IOError:
        pass
      return [output_api.PresubmitError("The tree is closed.",
                                        long_text=long_text)]
  except IOError:
    pass
  return []


def CheckPendingBuilds(input_api, output_api, url, max_pendings, ignored):
  try:
    connection = input_api.urllib2.urlopen(url)
    raw_data = connection.read()
    connection.close()
    try:
      import simplejson
      data = simplejson.loads(raw_data)
    except ImportError:
      # simplejson is much safer. But we should be just fine enough with that:
      data = eval(raw_data.replace('null', 'None'))
    out = []
    for (builder_name, builder)  in data.iteritems():
      if builder_name in ignored:
        continue
      pending_builds_len = len(builder.get('pending_builds', []))
      if pending_builds_len > max_pendings:
        out.append('%s has %d build(s) pending' %
                   (builder_name, pending_builds_len))
    if out:
      return [output_api.PresubmitPromptWarning(
          'Build(s) pending. It is suggested to wait that no more than %d '
              'builds are pending.' % max_pendings,
          long_text='\n'.join(out))]
  except IOError:
    # Silently pass.
    pass
  return []


def GetPreferredTrySlaves():
  return ['win', 'linux', 'mac']
