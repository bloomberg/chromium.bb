// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Whether task manager shows 'Private Memory' instead of 'Phsical Memory'.
 * @const
 */
var USE_PRIVATE_MEM = false;
// <if expr="(is_linux or pp_ifdef('chromeos'))">
// On Linux and ChromeOS, this is true because calculating Phsical Memory is
// slow.
USE_PRIVATE_MEM = true;
// </if>

/*
 * Default columns (column_id, label_id, width, is_default)
 * @const
 */
var DEFAULT_COLUMNS = [
  ['title', 'taskColumn', 300, true],
  ['profileName', 'profileNameColumn', 120, false],
  ['physicalMemory', 'physicalMemColumn', 80, !USE_PRIVATE_MEM],
  ['sharedMemory', 'sharedMemColumn', 80, false],
  ['privateMemory', 'privateMemColumn', 80, USE_PRIVATE_MEM],
  ['cpuUsage', 'cpuColumn', 80, true],
  ['networkUsage', 'netColumn', 85, true],
  ['processId', 'processIDColumn', 100, false],
  ['webCoreImageCacheSize', 'webcoreImageCacheColumn', 120, false],
  ['webCoreScriptsCacheSize', 'webcoreScriptsCacheColumn', 120, false],
  ['webCoreCSSCacheSize', 'webcoreCSSCacheColumn', 120, false],
  ['fps', 'fpsColumn', 50, true],
  ['sqliteMemoryUsed', 'sqliteMemoryUsedColumn', 80, false],
  ['goatsTeleported', 'goatsTeleportedColumn', 80, false],
  ['v8MemoryAllocatedSize', 'javascriptMemoryAllocatedColumn', 120, false],
];

/*
 * Height of each tasks. It is 20px, which is also defined in CSS.
 * @const
 */
var HEIGHT_OF_TASK = 20;

var COMMAND_CONTEXTMENU_COLUMN_PREFIX = 'columnContextMenu';
var COMMAND_CONTEXTMENU_TABLE_PREFIX = 'tableContextMenu';

var ENABLED_COLUMNS_KEY = 'enabledColumns';

var DEFAULT_SORT_COLUMN = 'cpuUsage';
var DEFAULT_SORT_DIRECTION = 'desc';
