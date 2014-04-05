# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import webapp2

application = webapp2.WSGIApplication([
  webapp2.Route('/', webapp2.RedirectHandler, defaults={
    '_uri': 'http://developer.chrome.com/native-client'}),
], debug=True)
