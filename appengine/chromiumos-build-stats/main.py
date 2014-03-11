# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import webapp2

import stats

# Application configuration.
URLS = [
  ('/', stats.MainPage),
  ('/stats', stats.MainPage),
  ('/upload_command_stats', stats.PostPage),
]
app = webapp2.WSGIApplication(URLS, debug=True)
