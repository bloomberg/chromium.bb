// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for objects sent from C++ to chrome://history.
 * @externs
 */

/**
 * The type of the history result object. The definition is based on
 * chrome/browser/ui/webui/history_ui.cc:
 *     BrowsingHistoryHandler::HistoryEntry::ToValue()
 * @typedef {{allTimestamps: Array<number>,
 *            blockedVisit: boolean,
 *            dateRelativeDay: string,
 *            dateShort: string,
 *            dateTimeOfDay: string,
 *            deviceName: string,
 *            deviceType: string,
 *            domain: string,
 *            hostFilteringBehavior: number,
 *            snippet: string,
 *            starred: boolean,
 *            time: number,
 *            title: string,
 *            url: string}}
 */
var HistoryEntry;

/**
 * The type of the history results info object. The definition is based on
 * chrome/browser/ui/webui/history_ui.cc:
 *     BrowsingHistoryHandler::QueryComplete()
 * @typedef {{finished: boolean,
 *            hasSyncedResults: boolean,
 *            queryEndTime: string,
 *            queryStartTime: string,
 *            term: string}}
 */
var HistoryQuery;
