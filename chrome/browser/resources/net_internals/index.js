// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="util.js"/>
<include src="table_printer.js"/>
<include src="view.js"/>
<include src="tab_switcher_view.js"/>
<include src="import_view.js"/>
<include src="capture_view.js"/>
<include src="export_view.js"/>
<include src="http_cache_view.js"/>
<include src="test_view.js"/>
<include src="hsts_view.js"/>
<include src="browser_bridge.js"/>
<include src="source_tracker.js"/>
<include src="resizable_vertical_split_view.js"/>
<include src="main.js"/>
<include src="time_util.js"/>
<include src="log_util.js"/>
<include src="dns_view.js"/>
<include src="source_row.js"/>
<include src="events_view.js"/>
<include src="details_view.js"/>
<include src="source_entry.js"/>
<include src="horizontal_scrollbar_view.js"/>
<include src="top_mid_bottom_view.js"/>
<include src="timeline_data_series.js"/>
<include src="timeline_graph_view.js"/>
<include src="timeline_view.js"/>
<include src="log_view_painter.js"/>
<include src="log_grouper.js"/>
<include src="proxy_view.js"/>
<include src="socket_pool_wrapper.js"/>
<include src="sockets_view.js"/>
<include src="spdy_view.js"/>
<include src="service_providers_view.js"/>
<include src="http_throttling_view.js"/>
<include src="logs_view.js"/>
<include src="prerender_view.js"/>

document.addEventListener('DOMContentLoaded', function() {
  MainView.getInstance();  // from main.js
});
