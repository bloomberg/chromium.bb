// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The include directives are put into Javascript-style comments to prevent
// parsing errors in non-flattened mode. The flattener still sees them.
// Note that this makes the flattener to comment out the first line of the
// included file but that's all right since any javascript file should start
// with a copyright comment anyway.

// If you add a new dependency, you should update build files by rerunning
// gyp. Otherwise, you'll be bitten by a dependency issue like:
//
// 1) You add a new dependency to "whatever.js"
// 2) You make changes in "whatever.js"
// 3) Rebuild "resources.pak" and open Files.app
// 4) You don't see the changes in "whatever.js". Why is that?
//
// Because the dependencies are computed at gyp time, the existing build
// files don't know that "resources.pak" now has a dependency to
// "whatever.js". You should rerun gyp to let the build files know.
//
// //metrics.js initiates load performance tracking
// //so we want to parse it as early as possible.
//<include src="metrics.js"/>
//
//<include src="../../image_loader/client.js"/>
//
//<include src="../../../../../ui/webui/resources/js/load_time_data.js"/>
//<include src="../../../../../ui/webui/resources/js/cr.js"/>
//<include src="../../../../../ui/webui/resources/js/util.js"/>
//<include src="../../../../../ui/webui/resources/js/i18n_template_no_process.js"/>
//
//<include src="../../../../../ui/webui/resources/js/event_tracker.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/event_target.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/touch_handler.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/array_data_model.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/dialogs.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/list_item.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/list_selection_model.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/list_single_selection_model.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/list_selection_controller.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/list.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/tree.js"/>
//<include src="../../../../../ui/webui/resources/css/tree.css.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/autocomplete_list.js"/>
//
//<include src="../../../../../ui/webui/resources/js/cr/ui/splitter.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/table/table_splitter.js"/>
//
//<include src="../../../../../ui/webui/resources/js/cr/ui/table/table_column.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/table/table_column_model.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/table/table_header.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/table/table_list.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/table.js"/>
//
//<include src="../../../../../ui/webui/resources/js/cr/ui/grid.js"/>
//
//<include src="../../../../../ui/webui/resources/js/cr/ui/command.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/position_util.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/menu_item.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/menu.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/menu_button.js"/>
//<include src="../../../../../ui/webui/resources/js/cr/ui/context_menu_handler.js"/>

(function() {
// 'strict mode' is invoked for this scope.

//<include src="combobutton.js"/>
//<include src="commandbutton.js"/>
//
//<include src="path_util.js"/>
//<include src="util.js"/>
//<include src="breadcrumbs_controller.js"/>
//<include src="butter_bar.js"/>
//<include src="directory_contents.js">
//<include src="directory_model.js"/>
//<include src="file_copy_manager_wrapper.js"/>
//<include src="drive_banners.js" />
//<include src="file_grid.js"/>
//<include src="file_manager.js"/>
//<include src="file_manager_pyauto.js"/>
//<include src="file_selection.js"/>
//<include src="file_table.js"/>
//<include src="file_tasks.js"/>
//<include src="file_transfer_controller.js"/>
//<include src="file_type.js"/>
//<include src="sidebar.js"/>
//<include src="volume_manager.js"/>
//<include src="media/media_util.js"/>
//<include src="metadata/metadata_cache.js"/>
//<include src="default_action_dialog.js"/>
//<include src="file_manager_commands.js"/>
// // For accurate load performance tracking place main.js should be
// // the last include to include.
//<include src="main.js"/>

// Exports
window.FileCopyManagerWrapper = FileCopyManagerWrapper;

})();
