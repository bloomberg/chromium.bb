# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

class TemplateUtility(object):
  def RenderTemplate(self, template, json_str, partials=None):
    if partials is None:
      partials = []
    # TODO(cduvall) error handling
    result = template.render(json.loads(json_str), {'templates': partials})
    return result.text
