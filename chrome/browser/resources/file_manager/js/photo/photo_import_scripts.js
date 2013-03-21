// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The include directives are put into Javascript-style comments to prevent
// parsing errors in non-flattened mode. The flattener still sees them.
// Note that this makes the flattener to comment out the first line of the
// included file but that's all right since any javascript file should start
// with a copyright comment anyway.

//<include src="../../../image_loader/client.js"/>

//<include src="../../../../../../ui/webui/resources/js/load_time_data.js"/>
//<include src="../../../../../../ui/webui/resources/js/util.js"/>
//<include src="../../../../../../ui/webui/resources/js/i18n_template_no_process.js"/>

//<include src="../../../../../../ui/webui/resources/js/cr.js"/>
//<include src="../../../../../../ui/webui/resources/js/event_tracker.js"/>
//<include src="../../../../../../ui/webui/resources/js/cr/ui.js"/>
//<include src="../../../../../../ui/webui/resources/js/cr/event_target.js"/>
//<include src="../../../../../../ui/webui/resources/js/cr/ui/touch_handler.js"/>
//<include src="../../../../../../ui/webui/resources/js/cr/ui/array_data_model.js"/>
//<include src="../../../../../../ui/webui/resources/js/cr/ui/dialogs.js"/>
//<include src="../../../../../../ui/webui/resources/js/cr/ui/list_item.js"/>
//<include src="../../../../../../ui/webui/resources/js/cr/ui/list_selection_model.js"/>
//<include src="../../../../../../ui/webui/resources/js/cr/ui/list_single_selection_model.js"/>
//<include src="../../../../../../ui/webui/resources/js/cr/ui/list_selection_controller.js"/>
//<include src="../../../../../../ui/webui/resources/js/cr/ui/list.js"/>
//<include src="../../../../../../ui/webui/resources/js/cr/ui/grid.js"/>

(function() {
// 'strict mode' is invoked for this scope.

//<include src="../util.js"/>
//<include src="../file_type.js"/>
//<include src="../directory_contents.js"/>
//<include src="../volume_manager.js"/>
//<include src="../path_util.js"/>
//<include src="../file_copy_manager_wrapper.js"/>
//<include src="../metadata/metadata_cache.js"/>
//<include src="../metrics.js"/>
//<include src="../image_editor/image_util.js"/>
//<include src="../media/media_util.js"/>

//<include src="importing_dialog.js"/>
//<include src="tile_view.js"/>
//<include src="photo_import.js"/>

})();
