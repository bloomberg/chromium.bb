# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

GITILES_BASE = 'https://chromium.googlesource.com/chromium/src/+'
GITILES_BRANCH_BASE = '%s/refs/branch-heads' % GITILES_BASE

GITHUB_REPOS = 'https://api.github.com/repos'
GITHUB_BASE = 'https://github.com/GoogleChrome/chrome-app-samples/tree/master'
RAW_GITHUB_BASE = ('https://github.com/GoogleChrome/chrome-app-samples/raw/'
                   'master')
OMAHA_PROXY_URL = 'http://omahaproxy.appspot.com/json'
# TODO(kalman): Change this to http://omahaproxy.appspot.com/history.json
OMAHA_HISTORY = 'http://10.omahaproxy-hrd.appspot.com/history.json'
OMAHA_DEV_HISTORY = '%s?channel=dev&os=win&json=1' % OMAHA_HISTORY
SVN_URL = 'http://src.chromium.org/chrome'
VIEWVC_URL = 'http://src.chromium.org/viewvc/chrome'
EXTENSIONS_SAMPLES = ('http://src.chromium.org/viewvc/chrome/trunk/src/chrome/'
                      'common/extensions/docs/examples')
CODEREVIEW_SERVER = 'https://codereview.chromium.org'
