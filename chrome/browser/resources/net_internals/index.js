// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="util.js"/>
<include src="view.js"/>
<include src="tabswitcherview.js"/>
<include src="dataview.js"/>
<include src="httpcacheview.js"/>
<include src="testview.js"/>
<include src="hstsview.js"/>
<include src="browserbridge.js"/>
<include src="sourcetracker.js"/>
<include src="main.js"/>
<include src="dnsview.js"/>
<include src="sourcerow.js"/>
<include src="eventsview.js"/>
<include src="detailsview.js"/>
<include src="sourceentry.js"/>
<include src="resizableverticalsplitview.js"/>
<include src="topmidbottomview.js"/>
<include src="timelineviewpainter.js"/>
<include src="logviewpainter.js"/>
<include src="loggrouper.js"/>
<include src="proxyview.js"/>
<include src="socketpoolwrapper.js"/>
<include src="socketsview.js"/>
<include src="spdyview.js"/>
<include src="serviceprovidersview.js"/>
<include src="httpthrottlingview.js"/>
<include src="logsview.js"/>
<include src="prerenderview.js"/>

document.addEventListener('DOMContentLoaded', function () {
  $('reloaded-link').addEventListener('click', function () {
    history.go(0);
  });
  onLoaded();  // from main.js
});
