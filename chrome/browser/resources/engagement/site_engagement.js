// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

define('main', [
    'mojo/public/js/connection',
    'chrome/browser/ui/webui/engagement/site_engagement.mojom',
    'content/public/renderer/service_provider',
], function(connection, siteEngagementMojom, serviceProvider) {
  return function() {
    var uiHandler = connection.bindHandleToProxy(
        serviceProvider.connectToService(
            siteEngagementMojom.SiteEngagementUIHandler.name),
        siteEngagementMojom.SiteEngagementUIHandler);

    var updateEngagementTable = function() {
      // Populate engagement table.
      uiHandler.getSiteEngagementInfo().then(function(response) {
        // Round each score to 2 decimal places.
        response.info.forEach(function(x) {
          x.score = Number(Math.round(x.score * 100) / 100);
        });
        $('engagement-table').engagementInfo = response.info;
      });

      setTimeout(updateEngagementTable, 2000);
    };
    updateEngagementTable();
  };
});
