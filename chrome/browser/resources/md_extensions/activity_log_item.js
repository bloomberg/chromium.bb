// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  const ActivityLogItem = Polymer({
    is: 'activity-log-item',

    properties: {
      /**
       * The underlying ExtensionActivity that provides data for the
       * ActivityLogItem displayed.
       * @type {chrome.activityLogPrivate.ExtensionActivity}
       */
      data: Object,
    },
  });

  return {ActivityLogItem: ActivityLogItem};
});
