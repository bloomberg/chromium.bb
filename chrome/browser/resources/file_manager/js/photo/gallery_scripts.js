// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The include directives are put into Javascript-style comments to prevent
// parsing errors in non-flattened mode. The flattener still sees them.
// Note that this makes the flattener to comment out the first line of the
// included file but that's all right since any javascript file should start
// with a copyright comment anyway.

//<include src="../metrics.js">

//<include src="../../../image_loader/client.js"/>

//<include src="../../../../../../ui/webui/resources/js/cr.js">
//<include src="../../../../../../ui/webui/resources/js/event_tracker.js">
//<include src="../../../../../../ui/webui/resources/js/load_time_data.js">

//<include src="../../../../../../ui/webui/resources/js/cr/ui.js">
//<include src="../../../../../../ui/webui/resources/js/cr/event_target.js">
//<include src="../../../../../../ui/webui/resources/js/cr/ui/touch_handler.js">
//<include src="../../../../../../ui/webui/resources/js/cr/ui/array_data_model.js">
//<include src="../../../../../../ui/webui/resources/js/cr/ui/dialogs.js">
//<include src="../../../../../../ui/webui/resources/js/cr/ui/list_item.js">
//<include src="../../../../../../ui/webui/resources/js/cr/ui/list_selection_model.js">
//<include src="../../../../../../ui/webui/resources/js/cr/ui/list_single_selection_model.js">
//<include src="../../../../../../ui/webui/resources/js/cr/ui/list_selection_controller.js">
//<include src="../../../../../../ui/webui/resources/js/cr/ui/list.js">
//<include src="../../../../../../ui/webui/resources/js/cr/ui/grid.js">

(function() {
// 'strict mode' is invoked for this scope.

//<include src="../file_type.js">
//<include src="../util.js">
//<include src="../volume_manager.js">

//<include src="../image_editor/image_util.js"/>
//<include src="../image_editor/viewport.js"/>
//<include src="../image_editor/image_buffer.js"/>
//<include src="../image_editor/image_view.js"/>
//<include src="../image_editor/commands.js"/>
//<include src="../image_editor/image_editor.js"/>
//<include src="../image_editor/image_transform.js"/>
//<include src="../image_editor/image_adjust.js"/>
//<include src="../image_editor/filter.js"/>
//<include src="../image_editor/image_encoder.js"/>
//<include src="../image_editor/exif_encoder.js"/>

//<include src="../media/media_controls.js"/>
//<include src="../media/media_util.js"/>
//<include src="../media/util.js"/>

//<include src="../metadata/metadata_cache.js"/>

//<include src="gallery.js">
//<include src="gallery_item.js">
//<include src="mosaic_mode.js">
//<include src="slide_mode.js">
//<include src="ribbon.js">

//<include src="gallery_testapi.js">

// Exports
window.ImageUtil = ImageUtil;
window.Gallery = Gallery;
window.beforeunload = beforeunload;
window.unload = unload;

})();
