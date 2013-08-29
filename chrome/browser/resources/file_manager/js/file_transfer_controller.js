// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Global (placed in the window object) variable name to hold internal
 * file dragging information. Needed to show visual feedback while dragging
 * since DataTransfer object is in protected state. Reachable from other
 * file manager instances.
 */
var DRAG_AND_DROP_GLOBAL_DATA = '__drag_and_drop_global_data';

/**
 * @param {HTMLDocument} doc Owning document.
 * @param {FileOperationManager} fileOperationManager File operation manager
 *     instance.
 * @param {MetadataCache} metadataCache Metadata cache service.
 * @param {DirectoryModel} directoryModel Directory model instance.
 * @constructor
 */
function FileTransferController(doc,
                                fileOperationManager,
                                metadataCache,
                                directoryModel) {
  this.document_ = doc;
  this.fileOperationManager_ = fileOperationManager;
  this.metadataCache_ = metadataCache;
  this.directoryModel_ = directoryModel;

  this.directoryModel_.getFileListSelection().addEventListener('change',
      this.onSelectionChanged_.bind(this));

  /**
   * DOM element to represent selected file in drag operation. Used if only
   * one element is selected.
   * @type {HTMLElement}
   * @private
   */
  this.preloadedThumbnailImageNode_ = null;

  /**
   * File objects for seletced files.
   *
   * @type {Array.<File>}
   * @private
   */
  this.selectedFileObjects_ = [];

  /**
   * Drag selector.
   * @type {DragSelector}
   * @private
   */
  this.dragSelector_ = new DragSelector();
}

FileTransferController.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * @this {FileTransferController}
   * @param {cr.ui.List} list Items in the list will be draggable.
   */
  attachDragSource: function(list) {
    list.style.webkitUserDrag = 'element';
    list.addEventListener('dragstart', this.onDragStart_.bind(this, list));
    list.addEventListener('dragend', this.onDragEnd_.bind(this, list));
  },

  /**
   * @this {FileTransferController}
   * @param {cr.ui.List} list List itself and its directory items will could
   *                          be drop target.
   * @param {boolean=} opt_onlyIntoDirectories If true only directory list
   *     items could be drop targets. Otherwise any other place of the list
   *     accetps files (putting it into the current directory).
   */
  attachFileListDropTarget: function(list, opt_onlyIntoDirectories) {
    list.addEventListener('dragover', this.onDragOver_.bind(this,
        !!opt_onlyIntoDirectories, list));
    list.addEventListener('dragenter',
        this.onDragEnterFileList_.bind(this, list));
    list.addEventListener('dragleave', this.onDragLeave_.bind(this, list));
    list.addEventListener('drop',
        this.onDrop_.bind(this, !!opt_onlyIntoDirectories));
  },

  /**
   * @this {FileTransferController}
   * @param {DirectoryTree} tree Its sub items will could be drop target.
   */
  attachTreeDropTarget: function(tree) {
    tree.addEventListener('dragover', this.onDragOver_.bind(this, true, tree));
    tree.addEventListener('dragenter', this.onDragEnterTree_.bind(this, tree));
    tree.addEventListener('dragleave', this.onDragLeave_.bind(this, tree));
    tree.addEventListener('drop', this.onDrop_.bind(this, true));
  },

  /**
   * @this {FileTransferController}
   * @param {NavigationList} tree Its sub items will could be drop target.
   */
  attachNavigationListDropTarget: function(list) {
    list.addEventListener('dragover',
        this.onDragOver_.bind(this, true /* onlyIntoDirectories */, list));
    list.addEventListener('dragenter',
        this.onDragEnterVolumesList_.bind(this, list));
    list.addEventListener('dragleave', this.onDragLeave_.bind(this, list));
    list.addEventListener('drop',
        this.onDrop_.bind(this, true /* onlyIntoDirectories */));
  },

  /**
   * Attach handlers of copy, cut and paste operations to the document.
   *
   * @this {FileTransferController}
   */
  attachCopyPasteHandlers: function() {
    this.document_.addEventListener('beforecopy',
                                    this.onBeforeCopy_.bind(this));
    this.document_.addEventListener('copy',
                                    this.onCopy_.bind(this));
    this.document_.addEventListener('beforecut',
                                    this.onBeforeCut_.bind(this));
    this.document_.addEventListener('cut',
                                    this.onCut_.bind(this));
    this.document_.addEventListener('beforepaste',
                                    this.onBeforePaste_.bind(this));
    this.document_.addEventListener('paste',
                                    this.onPaste_.bind(this));
    this.copyCommand_ = this.document_.querySelector('command#copy');
  },

  /**
   * Write the current selection to system clipboard.
   *
   * @this {FileTransferController}
   * @param {DataTransfer} dataTransfer DataTransfer from the event.
   * @param {string} effectAllowed Value must be valid for the
   *     |dataTransfer.effectAllowed| property ('move', 'copy', 'copyMove').
   */
  cutOrCopy_: function(dataTransfer, effectAllowed) {
    // Tag to check it's filemanager data.
    dataTransfer.setData('fs/tag', 'filemanager-data');
    dataTransfer.setData('fs/sourceRoot',
                         this.directoryModel_.getCurrentRootPath());
    var sourcePaths =
        this.selectedEntries_.map(function(e) { return e.fullPath; });
    dataTransfer.setData('fs/sources', sourcePaths.join('\n'));
    dataTransfer.effectAllowed = effectAllowed;
    dataTransfer.setData('fs/effectallowed', effectAllowed);

    for (var i = 0; i < this.selectedFileObjects_.length; i++) {
      dataTransfer.items.add(this.selectedFileObjects_[i]);
    }
  },

  /**
   * Extracts source root from the |dataTransfer| object.
   *
   * @this {FileTransferController}
   * @param {DataTransfer} dataTransfer DataTransfer object from the event.
   * @return {string} Path or empty string (if unknown).
   */
  getSourceRoot_: function(dataTransfer) {
    var sourceRoot = dataTransfer.getData('fs/sourceRoot');
    if (sourceRoot)
      return sourceRoot;

    // |dataTransfer| in protected mode.
    if (window[DRAG_AND_DROP_GLOBAL_DATA])
      return window[DRAG_AND_DROP_GLOBAL_DATA].sourceRoot;

    // Dragging from other tabs/windows.
    var views = chrome && chrome.extension ? chrome.extension.getViews() : [];
    for (var i = 0; i < views.length; i++) {
      if (views[i][DRAG_AND_DROP_GLOBAL_DATA])
        return views[i][DRAG_AND_DROP_GLOBAL_DATA].sourceRoot;
    }

    // Unknown source.
    return '';
  },

  /**
   * Queue up a file copy operation based on the current system clipboard.
   *
   * @this {FileTransferController}
   * @param {DataTransfer} dataTransfer System data transfer object.
   * @param {string=} opt_destinationPath Paste destination.
   * @param {string=} opt_effect Desired drop/paste effect. Could be
   *     'move'|'copy' (default is copy). Ignored if conflicts with
   *     |dataTransfer.effectAllowed|.
   * @return {string} Either "copy" or "move".
   */
  paste: function(dataTransfer, opt_destinationPath, opt_effect) {
    var sourcePaths = (dataTransfer.getData('fs/sources') || '').split('\n');
    var destinationPath = opt_destinationPath ||
                          this.currentDirectoryContentPath;
    // effectAllowed set in copy/pase handlers stay uninitialized. DnD handlers
    // work fine.
    var effectAllowed = dataTransfer.effectAllowed != 'uninitialized' ?
        dataTransfer.effectAllowed : dataTransfer.getData('fs/effectallowed');
    var toMove = effectAllowed == 'move' ||
        (effectAllowed == 'copyMove' && opt_effect == 'move');

    // Start the pasting operation.
    this.fileOperationManager_.paste(sourcePaths, destinationPath, toMove);
    return toMove ? 'move' : 'copy';
  },

  /**
   * Preloads an image thumbnail for the specified file entry.
   *
   * @this {FileTransferController}
   * @param {Entry} entry Entry to preload a thumbnail for.
   */
  preloadThumbnailImage_: function(entry) {
    var imageUrl = entry.toURL();
    var metadataTypes = 'thumbnail|filesystem';
    var thumbnailContainer = this.document_.createElement('div');
    this.preloadedThumbnailImageNode_ = thumbnailContainer;
    this.preloadedThumbnailImageNode_.className = 'img-container';
    this.metadataCache_.get(
        imageUrl,
        metadataTypes,
        function(metadata) {
          new ThumbnailLoader(imageUrl,
                              ThumbnailLoader.LoaderType.IMAGE,
                              metadata).
              load(thumbnailContainer,
                   ThumbnailLoader.FillMode.FILL);
        }.bind(this));
  },

  /**
   * Renders a drag-and-drop thumbnail.
   *
   * @this {FileTransferController}
   * @return {HTMLElement} Element containing the thumbnail.
   */
  renderThumbnail_: function() {
    var length = this.selectedEntries_.length;

    var container = this.document_.querySelector('#drag-container');
    var contents = this.document_.createElement('div');
    contents.className = 'drag-contents';
    container.appendChild(contents);

    var thumbnailImage;
    if (this.preloadedThumbnailImageNode_)
      thumbnailImage = this.preloadedThumbnailImageNode_.querySelector('img');

    // Option 1. Multiple selection, render only a label.
    if (length > 1) {
      var label = this.document_.createElement('div');
      label.className = 'label';
      label.textContent = strf('DRAGGING_MULTIPLE_ITEMS', length);
      contents.appendChild(label);
      return container;
    }

    // Option 2. Thumbnail image available, then render it without
    // a label.
    if (thumbnailImage) {
      thumbnailImage.classList.add('drag-thumbnail');
      contents.classList.add('for-image');
      contents.appendChild(this.preloadedThumbnailImageNode_);
      return container;
    }

    // Option 3. Thumbnail not available. Render an icon and a label.
    var entry = this.selectedEntries_[0];
    var icon = this.document_.createElement('div');
    icon.className = 'detail-icon';
    icon.setAttribute('file-type-icon', FileType.getIcon(entry));
    contents.appendChild(icon);
    var label = this.document_.createElement('div');
    label.className = 'label';
    label.textContent = entry.name;
    contents.appendChild(label);
    return container;
  },

  /**
   * @this {FileTransferController}
   * @param {cr.ui.List} list Drop target list
   * @param {Event} event A dragstart event of DOM.
   */
  onDragStart_: function(list, event) {
    // Check if a drag selection should be initiated or not.
    // TODO(hirono): Support drag selection on the grid view. crbug.com/247278
    if (list.id == 'file-list' && list.parentNode.id == 'detail-table') {
      if (list.parentNode.shouldStartDragSelection(event)) {
        this.dragSelector_.startDragSelection(list, event);
        return;
      }
    }

    // Nothing selected.
    if (!this.selectedEntries_.length) {
      event.preventDefault();
      return;
    }

    var dt = event.dataTransfer;

    if (this.canCopyOrDrag_(dt)) {
      if (this.canCutOrDrag_(dt))
        this.cutOrCopy_(dt, 'copyMove');
      else
        this.cutOrCopy_(dt, 'copy');
    } else {
      event.preventDefault();
      return;
    }

    var dragThumbnail = this.renderThumbnail_();
    dt.setDragImage(dragThumbnail, 1000, 1000);

    window[DRAG_AND_DROP_GLOBAL_DATA] = {
      sourceRoot: this.directoryModel_.getCurrentRootPath()
    };
  },

  /**
   * @this {FileTransferController}
   * @param {cr.ui.List} list Drop target list.
   * @param {Event} event A dragend event of DOM.
   */
  onDragEnd_: function(list, event) {
    var container = this.document_.querySelector('#drag-container');
    container.textContent = '';
    this.clearDropTarget_();
    delete window[DRAG_AND_DROP_GLOBAL_DATA];
  },

  /**
   * @this {FileTransferController}
   * @param {boolean} onlyIntoDirectories True if the drag is only into
   *     directoris.
   * @param {cr.ui.List} list Drop target list.
   * @param {Event} event A dragover event of DOM.
   */
  onDragOver_: function(onlyIntoDirectories, list, event) {
    event.preventDefault();
    var path = this.destinationPath_ ||
        (!onlyIntoDirectories && this.currentDirectoryContentPath);
    event.dataTransfer.dropEffect = this.selectDropEffect_(event, path);
    event.preventDefault();
  },

  /**
   * @this {FileTransferController}
   * @param {cr.ui.List} list Drop target list.
   * @param {Event} event A dragenter event of DOM.
   */
  onDragEnterFileList_: function(list, event) {
    event.preventDefault();  // Required to prevent the cursor flicker.
    this.lastEnteredTarget_ = event.target;
    var item = list.getListItemAncestor(event.target);
    item = item && list.isItem(item) ? item : null;
    if (item == this.dropTarget_)
      return;

    var entry = item && list.dataModel.item(item.listIndex);
    if (entry) {
      this.setDropTarget_(item, entry.isDirectory, event.dataTransfer,
          entry.fullPath);
    } else {
      this.clearDropTarget_();
    }
  },

  /**
   * @this {FileTransferController}
   * @param {DirectoryTree} tree Drop target tree.
   * @param {Event} event A dragenter event of DOM.
   */
  onDragEnterTree_: function(tree, event) {
    event.preventDefault();  // Required to prevent the cursor flicker.
    this.lastEnteredTarget_ = event.target;
    var item = event.target;
    while (item && !(item instanceof DirectoryItem)) {
      item = item.parentNode;
    }

    if (item == this.dropTarget_)
      return;

    var entry = item && item.entry;
    if (entry) {
      this.setDropTarget_(item, entry.isDirectory, event.dataTransfer,
          entry.fullPath);
    } else {
      this.clearDropTarget_();
    }
  },

  /**
   * @this {FileTransferController}
   * @param {NavigationList} list Drop target list.
   * @param {Event} event A dragenter event of DOM.
   */
  onDragEnterVolumesList_: function(list, event) {
    event.preventDefault();  // Required to prevent the cursor flicker.
    this.lastEnteredTarget_ = event.target;
    var item = list.getListItemAncestor(event.target);
    item = item && list.isItem(item) ? item : null;
    if (item == this.dropTarget_)
      return;

    var path = item && list.dataModel.item(item.listIndex).path;
    if (path)
      this.setDropTarget_(item, true /* directory */, event.dataTransfer, path);
    else
      this.clearDropTarget_();
  },

  /**
   * @this {FileTransferController}
   * @param {cr.ui.List} list Drop target list.
   * @param {Event} event A dragleave event of DOM.
   */
  onDragLeave_: function(list, event) {
    // If mouse moves from one element to another the 'dragenter'
    // event for the new element comes before the 'dragleave' event for
    // the old one. In this case event.target != this.lastEnteredTarget_
    // and handler of the 'dragenter' event has already caried of
    // drop target. So event.target == this.lastEnteredTarget_
    // could only be if mouse goes out of listened element.
    if (event.target == this.lastEnteredTarget_) {
      this.clearDropTarget_();
      this.lastEnteredTarget_ = null;
    }
  },

  /**
   * @this {FileTransferController}
   * @param {boolean} onlyIntoDirectories True if the drag is only into
   *     directories.
   * @param {Event} event A dragleave event of DOM.
   */
  onDrop_: function(onlyIntoDirectories, event) {
    if (onlyIntoDirectories && !this.dropTarget_)
      return;
    var destinationPath = this.destinationPath_ ||
                          this.currentDirectoryContentPath;
    if (!this.canPasteOrDrop_(event.dataTransfer, destinationPath))
      return;
    event.preventDefault();
    this.paste(event.dataTransfer, destinationPath,
               this.selectDropEffect_(event, destinationPath));
    this.clearDropTarget_();
  },

  /**
   * Sets the drop target.
   * @this {FileTransferController}
   * @param {Element} domElement Target of the drop.
   * @param {boolean} isDirectory If the target is a directory.
   * @param {DataTransfer} dataTransfer Data transfer object.
   * @param {string} destinationPath Destination path.
   */
  setDropTarget_: function(domElement, isDirectory, dataTransfer,
                           destinationPath) {
    if (this.dropTarget_ == domElement)
      return;

    // Remove the old drop target.
    this.clearDropTarget_();

    // Set the new drop target.
    this.dropTarget_ = domElement;

    if (!domElement ||
        !isDirectory ||
        !this.canPasteOrDrop_(dataTransfer, destinationPath)) {
      return;
    }

    // Add accept class if the domElement can accept the drag.
    domElement.classList.add('accepts');
    this.destinationPath_ = destinationPath;

    // Start timer changing the directory.
    this.navigateTimer_ = setTimeout(function() {
      if (domElement instanceof DirectoryItem)
        // Do custom action.
        (/** @type {DirectoryItem} */ domElement).doDropTargetAction();
      this.directoryModel_.changeDirectory(destinationPath);
    }.bind(this), 2000);
  },

  /**
   * Clears the drop target.
   * @this {FileTransferController}
   */
  clearDropTarget_: function() {
    if (this.dropTarget_ && this.dropTarget_.classList.contains('accepts'))
      this.dropTarget_.classList.remove('accepts');
    this.dropTarget_ = null;
    this.destinationPath_ = null;
    if (this.navigateTimer_ !== undefined) {
      clearTimeout(this.navigateTimer_);
      this.navigateTimer_ = undefined;
    }
  },

  /**
   * @this {FileTransferController}
   * @return {boolean} Returns false if {@code <input type="text">} element is
   *     currently active. Otherwise, returns true.
   */
  isDocumentWideEvent_: function() {
    return this.document_.activeElement.nodeName.toLowerCase() != 'input' ||
        this.document_.activeElement.type.toLowerCase() != 'text';
  },

  /**
   * @this {FileTransferController}
   */
  onCopy_: function(event) {
    if (!this.isDocumentWideEvent_() ||
        !this.canCopyOrDrag_()) {
      return;
    }
    event.preventDefault();
    this.cutOrCopy_(event.clipboardData, 'copy');
    this.notify_('selection-copied');
  },

  /**
   * @this {FileTransferController}
   */
  onBeforeCopy_: function(event) {
    if (!this.isDocumentWideEvent_())
      return;

    // queryCommandEnabled returns true if event.defaultPrevented is true.
    if (this.canCopyOrDrag_())
      event.preventDefault();
  },

  /**
   * @this {FileTransferController}
   * @return {boolean} Returns true if some files are selected and all the file
   *     on drive is available to be copied. Otherwise, returns false.
   */
  canCopyOrDrag_: function() {
    if (this.isOnDrive &&
        this.directoryModel_.isDriveOffline() &&
        !this.allDriveFilesAvailable)
      return false;
    return this.selectedEntries_.length > 0;
  },

  /**
   * @this {FileTransferController}
   */
  onCut_: function(event) {
    if (!this.isDocumentWideEvent_() ||
        !this.canCutOrDrag_()) {
      return;
    }
    event.preventDefault();
    this.cutOrCopy_(event.clipboardData, 'move');
    this.notify_('selection-cut');
  },

  /**
   * @this {FileTransferController}
   */
  onBeforeCut_: function(event) {
    if (!this.isDocumentWideEvent_())
      return;
    // queryCommandEnabled returns true if event.defaultPrevented is true.
    if (this.canCutOrDrag_())
      event.preventDefault();
  },

  /**
   * @this {FileTransferController}
   * @return {boolean} Returns true if some files are selected and all the file
   *     on drive is available to be cut. Otherwise, returns false.
   */
  canCutOrDrag_: function() {
    return !this.readonly && this.canCopyOrDrag_();
  },

  /**
   * @this {FileTransferController}
   */
  onPaste_: function(event) {
    // Need to update here since 'beforepaste' doesn't fire.
    if (!this.isDocumentWideEvent_() ||
        !this.canPasteOrDrop_(event.clipboardData,
                              this.currentDirectoryContentPath)) {
      return;
    }
    event.preventDefault();
    var effect = this.paste(event.clipboardData);

    // On cut, we clear the clipboard after the file is pasted/moved so we don't
    // try to move/delete the original file again.
    if (effect == 'move') {
      this.simulateCommand_('cut', function(event) {
        event.preventDefault();
        event.clipboardData.setData('fs/clear', '');
      });
    }
  },

  /**
   * @this {FileTransferController}
   */
  onBeforePaste_: function(event) {
    if (!this.isDocumentWideEvent_())
      return;
    // queryCommandEnabled returns true if event.defaultPrevented is true.
    if (this.canPasteOrDrop_(event.clipboardData,
                             this.currentDirectoryContentPath)) {
      event.preventDefault();
    }
  },

  /**
   * @this {FileTransferController}
   * @param {DataTransfer} dataTransfer Data transfer object.
   * @param {string?} destinationPath Destination path.
   * @return {boolean} Returns true if items stored in {@code dataTransfer} can
   *     be pasted to {@code destinationPath}. Otherwise, returns false.
   */
  canPasteOrDrop_: function(dataTransfer, destinationPath) {
    if (!destinationPath) {
      return false;
    }
    if (this.directoryModel_.isPathReadOnly(destinationPath)) {
      return false;
    }
    if (!dataTransfer.types || dataTransfer.types.indexOf('fs/tag') == -1) {
      return false;  // Unsupported type of content.
    }
    if (dataTransfer.getData('fs/tag') == '') {
      // Data protected. Other checks are not possible but it makes sense to
      // let the user try.
      return true;
    }

    var directories = dataTransfer.getData('fs/directories').split('\n').
                      filter(function(d) { return d != ''; });

    for (var i = 0; i < directories.length; i++) {
      if (destinationPath.substr(0, directories[i].length) == directories[i])
        return false;  // recursive paste.
    }

    return true;
  },

  /**
   * Execute paste command.
   *
   * @this {FileTransferController}
   * @return {boolean}  Returns true, the paste is success. Otherwise, returns
   *     false.
   */
  queryPasteCommandEnabled: function() {
    if (!this.isDocumentWideEvent_()) {
      return false;
    }

    // HACK(serya): return this.document_.queryCommandEnabled('paste')
    // should be used.
    var result;
    this.simulateCommand_('paste', function(event) {
      result = this.canPasteOrDrop_(event.clipboardData,
                                    this.currentDirectoryContentPath);
    }.bind(this));
    return result;
  },

  /**
   * Allows to simulate commands to get access to clipboard.
   *
   * @this {FileTransferController}
   * @param {string} command 'copy', 'cut' or 'paste'.
   * @param {function} handler Event handler.
   */
  simulateCommand_: function(command, handler) {
    var iframe = this.document_.querySelector('#command-dispatcher');
    var doc = iframe.contentDocument;
    doc.addEventListener(command, handler);
    doc.execCommand(command);
    doc.removeEventListener(command, handler);
  },

  /**
   * @this {FileTransferController}
   */
  onSelectionChanged_: function(event) {
    var entries = this.selectedEntries_;
    var files = this.selectedFileObjects_ = [];
    this.preloadedThumbnailImageNode_ = null;

    var fileEntries = [];
    for (var i = 0; i < entries.length; i++) {
      if (entries[i].isFile)
        fileEntries.push(entries[i]);
    }

    if (entries.length == 1) {
      // For single selection, the dragged element is created in advance,
      // otherwise an image may not be loaded at the time the 'dragstart' event
      // comes.
      this.preloadThumbnailImage_(entries[0]);
    }

    // File object must be prepeared in advance for clipboard operations
    // (copy, paste and drag). DataTransfer object closes for write after
    // returning control from that handlers so they may not have
    // asynchronous operations.
    var prepareFileObjects = function() {
      for (var i = 0; i < fileEntries.length; i++) {
        fileEntries[i].file(function(file) { files.push(file); });
      }
    };

    if (this.isOnDrive) {
      this.allDriveFilesAvailable = false;
      var urls = entries.map(function(e) { return e.toURL() });
      this.metadataCache_.get(
          urls, 'drive', function(props) {
        // We consider directories not available offline for the purposes of
        // file transfer since we cannot afford to recursive traversal.
        this.allDriveFilesAvailable =
            entries.filter(function(e) {return e.isDirectory}).length == 0 &&
            props.filter(function(p) {return !p.availableOffline}).length == 0;
        // |Copy| is the only menu item affected by allDriveFilesAvailable.
        // It could be open right now, update its UI.
        this.copyCommand_.disabled = !this.canCopyOrDrag_();

        if (this.allDriveFilesAvailable)
          prepareFileObjects();
      }.bind(this));
    } else {
      prepareFileObjects();
    }
  },

  /**
   * Path of directory that is displaying now.
   * If search result is displaying now, this is null.
   * @this {FileTransferController}
   * @return {string} Path of directry that is displaying now.
   */
  get currentDirectoryContentPath() {
    return this.directoryModel_.isSearching() ?
        null : this.directoryModel_.getCurrentDirPath();
  },

  /**
   * @this {FileTransferController}
   * @return {boolean} True if the current directory is read only.
   */
  get readonly() {
    return this.directoryModel_.isReadOnly();
  },

  /**
   * @this {FileTransferController}
   * @return {boolean} True if the current directory is on Drive.
   */
  get isOnDrive() {
    return PathUtil.isDriveBasedPath(this.directoryModel_.getCurrentRootPath());
  },

  /**
   * @this {FileTransferController}
   */
  notify_: function(eventName) {
    var self = this;
    // Set timeout to avoid recursive events.
    setTimeout(function() {
      cr.dispatchSimpleEvent(self, eventName);
    }, 0);
  },

  /**
   * @this {FileTransferController}
   * @return {Array.<Entry>} Array of the selected entries.
   */
  get selectedEntries_() {
    var list = this.directoryModel_.getFileList();
    var selectedIndexes = this.directoryModel_.getFileListSelection().
                               selectedIndexes;
    var entries = selectedIndexes.map(function(index) {
      return list.item(index);
    });

    // TODO(serya): Diagnostics for http://crbug/129642
    if (entries.indexOf(undefined) != -1) {
      var index = entries.indexOf(undefined);
      entries = entries.filter(function(e) { return !!e; });
      console.error('Invalid selection found: list items: ', list.length,
                    'wrong indexe value: ', selectedIndexes[index],
                    'Stack trace: ', new Error().stack);
    }
    return entries;
  },

  /**
   * @this {FileTransferController}
   * @return {string}  Returns the appropriate drop query type ('none', 'move'
   *     or copy') to the current modifiers status and the destination.
   */
  selectDropEffect_: function(event, destinationPath) {
    if (!destinationPath ||
        this.directoryModel_.isPathReadOnly(destinationPath))
      return 'none';
    if (event.dataTransfer.effectAllowed == 'copyMove' &&
        this.getSourceRoot_(event.dataTransfer) ==
            PathUtil.getRootPath(destinationPath) &&
        !event.ctrlKey) {
      return 'move';
    }
    if (event.dataTransfer.effectAllowed == 'copyMove' &&
        event.shiftKey) {
      return 'move';
    }
    return 'copy';
  },
};
