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
 * @param {FileCopyManager} copyManager Copy manager instance.
 * @param {DirectoryModel} directoryModel Directory model instance.
 * @constructor
 */
function FileTransferController(doc,
                                copyManager,
                                directoryModel) {
  this.document_ = doc;
  this.copyManager_ = copyManager;
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
  attachDropTarget: function(list, opt_onlyIntoDirectories) {
    list.addEventListener('dragover', this.onDragOver_.bind(this,
                          !!opt_onlyIntoDirectories, list));
    list.addEventListener('dragenter', this.onDragEnterList_.bind(this, list));
    list.addEventListener('dragleave', this.onDragLeave_.bind(this, list));
    list.addEventListener('drop', this.onDrop_.bind(this,
                          !!opt_onlyIntoDirectories));
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
   * @param {HTMLElement} breadcrumbsContainer Element which contains target
   *     breadcrumbs.
   */
  attachBreadcrumbsDropTarget: function(breadcrumbsController) {
    var container = breadcrumbsController.getContainer();
    container.addEventListener('dragover',
        this.onDragOver_.bind(this, true, null));
    container.addEventListener('dragenter',
        this.onDragEnterBreadcrumbs_.bind(this, breadcrumbsController));
    container.addEventListener('dragleave',
        this.onDragLeave_.bind(this, null));
    container.addEventListener('drop', this.onDrop_.bind(this, true));
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
    var directories = [];
    var files = [];
    var entries = this.selectedEntries_;
    for (var i = 0; i < entries.length; i++) {
      (entries[i].isDirectory ? directories : files).push(entries[i].fullPath);
    }

    // Tag to check it's filemanager data.
    dataTransfer.setData('fs/tag', 'filemanager-data');

    dataTransfer.setData('fs/isOnDrive', this.isOnDrive);
    if (this.currentDirectory)
      dataTransfer.setData('fs/sourceDir', this.currentDirectory.fullPath);
    dataTransfer.setData('fs/directories', directories.join('\n'));
    dataTransfer.setData('fs/files', files.join('\n'));
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
    var sourceDir = dataTransfer.getData('fs/sourceDir');
    if (sourceDir)
      return PathUtil.getRootPath(sourceDir);

    // For drive search, sourceDir will be set to null, so we should double
    // check that we are not on drive.
    // TODO(haruki): Investigate if this still is the case.
    if (dataTransfer.getData('fs/isOnDrive') == 'true')
      return RootDirectory.DRIVE;

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
    var destinationPath = opt_destinationPath ||
                          this.directoryModel_.getCurrentDirPath();
    // effectAllowed set in copy/pase handlers stay uninitialized. DnD handlers
    // work fine.
    var effectAllowed = dataTransfer.effectAllowed != 'uninitialized' ?
        dataTransfer.effectAllowed : dataTransfer.getData('fs/effectallowed');

    var toMove = effectAllowed == 'move' ||
        (effectAllowed == 'copyMove' && opt_effect == 'move');

    var operationInfo = {
      isCut: String(toMove),
      isOnDrive: dataTransfer.getData('fs/isOnDrive'),
      sourceDir: dataTransfer.getData('fs/sourceDir'),
      directories: dataTransfer.getData('fs/directories'),
      files: dataTransfer.getData('fs/files')
    };

    if (!toMove || operationInfo.sourceDir != destinationPath) {
      var targetOnDrive = (PathUtil.getRootType(destinationPath) ===
                           RootType.DRIVE);
      this.copyManager_.paste(operationInfo,
                              destinationPath,
                              targetOnDrive);
    } else {
      console.log('Ignore move into the same folder');
    }

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
    this.directoryModel_.getMetadataCache().get(
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
      contents.classList.add('drag-image-thumbnail');
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
        (!onlyIntoDirectories && this.directoryModel_.getCurrentDirPath());
    event.dataTransfer.dropEffect = this.selectDropEffect_(event, path);
    event.preventDefault();
  },

  /**
   * @this {FileTransferController}
   * @param {cr.ui.List} list Drop target list.
   * @param {Event} event A dragenter event of DOM.
   */
  onDragEnterList_: function(list, event) {
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
   * @param {HTMLElement} breadcrumbsContainer Element which contains target
   *     breadcrumbs.
   * @param {Event} event A dragenter event of DOM.
   */
  onDragEnterBreadcrumbs_: function(breadcrumbsContainer, event) {
    event.preventDefault();  // Required to prevent the cursor flicker.
    this.lastEnteredTarget_ = event.target;
    var path = breadcrumbsContainer.getTargetPath(event);
    if (!path)
      return;

    this.setDropTarget_(event.target, true, event.dataTransfer, path);
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
                          this.directoryModel_.getCurrentDirPath();
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

    /** @type {string?} */
    this.destinationPath_ = null;

    // Add accept class if the domElement can accept the drag.
    if (isDirectory &&
        this.canPasteOrDrop_(dataTransfer, destinationPath)) {
      domElement.classList.add('accepts');
      this.destinationPath_ = destinationPath;
    }

    // Remove the old drag target.
    this.clearDropTarget_();

    // Set the new drop target.
    this.dropTarget_ = domElement;

    // Start timer changing the directory.
    if (domElement && isDirectory && destinationPath &&
        this.canPasteOrDrop_(dataTransfer, destinationPath)) {
      this.navigateTimer_ = setTimeout(function() {
        this.directoryModel_.changeDirectory(destinationPath);
      }.bind(this), 2000);
    }
  },

  /**
   * Clears the drop target.
   * @this {FileTransferController}
   */
  clearDropTarget_: function() {
    if (this.dropTarget_ && this.dropTarget_.classList.contains('accepts'))
      this.dropTarget_.classList.remove('accepts');
    this.dropTarget_ = null;
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

    // queryCommandEnabled returns true if event.returnValue is false.
    event.returnValue = !this.canCopyOrDrag_();
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
    // queryCommandEnabled returns true if event.returnValue is false.
    event.returnValue = !this.canCutOrDrag_();
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
        !this.canPasteOrDrop_(event.clipboardData)) {
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
    // queryCommandEnabled returns true if event.returnValue is false.
    event.returnValue = !this.canPasteOrDrop_(event.clipboardData);
  },

  /**
   * @this {FileTransferController}
   * @param {DataTransfer} dataTransfer Data transfer object.
   * @param {string=} opt_destinationPath Destination path.
   * @return {boolean}  Returns true if items stored in {@code dataTransfer} can
   *     be pasted to {@code opt_destinationPath}. Otherwise, returns false.
   */
  canPasteOrDrop_: function(dataTransfer, opt_destinationPath) {
    var destinationPath = opt_destinationPath ||
                          this.directoryModel_.getCurrentDirPath();
    if (this.directoryModel_.isPathReadOnly(destinationPath)) {
      return false;
    }
    if (this.directoryModel_.isSearching())
      return false;

    if (!dataTransfer.types || dataTransfer.types.indexOf('fs/tag') == -1)
      return false;  // Unsupported type of content.
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
      result = this.canPasteOrDrop_(event.clipboardData);
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
      this.directoryModel_.getMetadataCache().get(
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
   * @this {FileTransferController}
   */
  get currentDirectory() {
    if (this.directoryModel_.isSearching() && this.isOnDrive)
      return null;
    return this.directoryModel_.getCurrentDirEntry();
  },

  /**
   * @this {FileTransferController}
   */
  get readonly() {
    return this.directoryModel_.isReadOnly();
  },

  /**
   * @this {FileTransferController}
   */
  get isOnDrive() {
    return this.directoryModel_.getCurrentRootType() === RootType.DRIVE ||
           this.directoryModel_.getCurrentRootType() === RootType.DRIVE_OFFLINE;
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
   * @type {Array.<Entry>}
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
