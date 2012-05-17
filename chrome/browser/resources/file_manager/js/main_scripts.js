// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The include directives are put into Javascript-style comments to prevent
// parsing errors in non-flattened mode. The flattener still sees them.
// Note that this makes the flattener to comment out the first line of the
// included file but that's all right since any javascript file should start
// with a copyright comment anyway.

// //metrics.js initiates load performance tracking
// //so we want to parse it as early as possible.
//<include src="metrics.js"/>
//
//<include src="../../shared/js/load_time_data.js"/>
//<include src="../../shared/js/util.js"/>
//<include src="../../shared/js/i18n_template_no_process.js"/>
//
//<include src="../../shared/js/cr.js"/>
//<include src="../../shared/js/event_tracker.js"/>
//<include src="../../shared/js/cr/ui.js"/>
//<include src="../../shared/js/cr/event_target.js"/>
//<include src="../../shared/js/cr/ui/touch_handler.js"/>
//<include src="../../shared/js/cr/ui/array_data_model.js"/>
//<include src="../../shared/js/cr/ui/dialogs.js"/>
//<include src="../../shared/js/cr/ui/list_item.js"/>
//<include src="../../shared/js/cr/ui/list_selection_model.js"/>
//<include src="../../shared/js/cr/ui/list_single_selection_model.js"/>
//<include src="../../shared/js/cr/ui/list_selection_controller.js"/>
//<include src="../../shared/js/cr/ui/list.js"/>
//
//<include src="../../shared/js/cr/ui/splitter.js"/>
//<include src="../../shared/js/cr/ui/table/table_splitter.js"/>
//
//<include src="../../shared/js/cr/ui/table/table_column.js"/>
//<include src="../../shared/js/cr/ui/table/table_column_model.js"/>
//<include src="../../shared/js/cr/ui/table/table_header.js"/>
//<include src="../../shared/js/cr/ui/table/table_list.js"/>
//<include src="../../shared/js/cr/ui/table.js"/>
//
//<include src="../../shared/js/cr/ui/grid.js"/>
//
//<include src="../../shared/js/cr/ui/command.js"/>
//<include src="../../shared/js/cr/ui/position_util.js"/>
//<include src="../../shared/js/cr/ui/menu_item.js"/>
//<include src="../../shared/js/cr/ui/menu.js"/>
//<include src="../../shared/js/cr/ui/menu_button.js"/>
//<include src="../../shared/js/cr/ui/context_menu_handler.js"/>
//
//<include src="combobutton.js"/>
//
//<include src="util.js"/>
//<include src="directory_model.js"/>
//<include src="file_copy_manager.js"/>
//<include src="file_manager.js"/>
//<include src="file_manager_pyauto.js"/>
//<include src="file_type.js"/>
//<include src="file_transfer_controller.js"/>
//<include src="metadata/metadata_provider.js"/>
//<include src="metadata/metadata_cache.js"/>
// // For accurate load performance tracking place main.js should be
// // the last include to include.
//<include src="main.js"/>
