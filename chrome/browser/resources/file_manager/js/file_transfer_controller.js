// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var MAX_DRAG_THUMBAIL_COUNT = 4;

/**
 * TODO(olege): Fix style warnings.
 */
function FileTransferController(fileList,
                                fileListSelection,
                                dragNodeConstructor,
                                copyManager,
                                directoryModel) {
  this.fileList_ = fileList;
  this.fileListSelection_ = fileListSelection;
  this.dragNodeConstructor_ = dragNodeConstructor;
  this.copyManager_ = copyManager;
  this.directoryModel_ = directoryModel;

  this.fileListSelection_.addEventListener('change',
      this.onSelectionChanged_.bind(this));

  /**
   * DOM elements to represent selected files in drag operation.
   * @type {Array.<Element>}
   */
  this.dragNodes_ = [];

  /**
   * File objects for seletced files.
   * @type {Array.<File>}
   */
  this.files_ = [];

}

FileTransferController.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * @param {cr.ui.List} list Items in the list will be draggable.
   */
  attachDragSource: function(list) {
    list.style.webkitUserDrag = 'element';
    list.addEventListener('dragstart', this.onDragStart_.bind(this, list));
    list.addEventListener('dragend', this.onDragEnd_.bind(this, list));
    list.addEventListener('drag', this.onDrag_.bind(this, list));
  },

  /**
   * @param {cr.ui.List} list List itself and its directory items will could
   *                          be drop target.
   */
  attachDropTarget: function(list) {
    list.addEventListener('dragover', this.onDragOver_.bind(this, list));
    list.addEventListener('dragenter', this.onDragEnter_.bind(this, list));
    list.addEventListener('dragleave', this.onDragLeave_.bind(this, list));
    list.addEventListener('drop', this.onDrop_.bind(this, list));
  },

  /**
   * Attach handlers of copy, cut and paste operations to the document.
   * @param {HTMLDocument} doc Command dispatcher.
   */
  attachCopyPasteHandlers: function(doc) {
    this.document_ = doc;
    doc.addEventListener('beforecopy', this.onBeforeCopy_.bind(this));
    doc.addEventListener('copy', this.onCopy_.bind(this));
    doc.addEventListener('beforecut', this.onBeforeCut_.bind(this));
    doc.addEventListener('cut', this.onCut_.bind(this));
    doc.addEventListener('beforepaste', this.onBeforePaste_.bind(this));
    doc.addEventListener('paste', this.onPaste_.bind(this));
  },

  /**
   * Write the current selection to system clipboard.
   *
   * @param {Clipboard} clipboard Clipboard from the event.
   * @param {string} effectAllowed Value must be valid for the
   *     |dataTransfer.effectAllowed| property ('move', 'copy', 'copyMove').
   */
  cutOrCopy: function(dataTransfer, effectAllowed) {
    var directories  = [];
    var files = [];
    var entries = this.selectedEntries_;
    for (var i = 0; i < entries.length; i++) {
      (entries[i].isDirectory ? directories : files).push(entries[i].fullPath);
    }

    // Tag to check it's filemanager data.
    dataTransfer.setData('fs/tag', 'filemanager-data');

    dataTransfer.setData('fs/isOnGData', this.isOnGData);
    dataTransfer.setData('fs/sourceDir',
                         this.currentDirectory.fullPath);
    dataTransfer.setData('fs/directories', directories.join('\n'));
    dataTransfer.setData('fs/files', files.join('\n'));
    dataTransfer.effectAllowed = effectAllowed;
    dataTransfer.setData('fs/effectallowed', effectAllowed);

    for (var i = 0; i < this.files_.length; i++) {
      dataTransfer.items.add(this.files_[i]);
    }
  },

  /**
   * Queue up a file copy operation based on the current system clipboard.
   * @param {DataTransfer} dataTransfer System data transfer object.
   * @param {DirectoryEntry=} opt_destination Paste destination.
   */
  paste: function(dataTransfer, opt_destination) {
    var destination = opt_destination || this.currentDirectory;
    // effectAllowed set in copy/pase handlers stay uninitialized. DnD handlers
    // work fine.
    var effectAllowed = dataTransfer.effectAllowed != 'uninitialized' ?
        dataTransfer.effectAllowed : dataTransfer.getData('fs/effectallowed');

    var toMove = effectAllowed == 'move' ||
        (effectAllowed == 'copyMove' && dataTransfer.dropEffect != 'copy');

    var operationInfo = {
      isCut: String(toMove),
      isOnGData: dataTransfer.getData('fs/isOnGData'),
      sourceDir: dataTransfer.getData('fs/sourceDir'),
      directories: dataTransfer.getData('fs/directories'),
      files: dataTransfer.getData('fs/files')
    };

    if (!toMove || operationInfo.sourceDir != destination.fullPath) {
      this.copyManager_.paste(operationInfo,
                              destination,
                              this.isOnGData);
    } else {
      console.log('Ignore move into the same folder');
    }

    return toMove ? 'move' : 'copy';
  },

  onDragStart_: function(list, event) {
    var dt = event.dataTransfer;
    var doc = list.ownerDocument;
    var container = doc.querySelector('#drag-image-container');
    var length = this.dragNodes_.length;
    for (var i = 0; i < length; i++) {
      var listItem = this.dragNodes_[i];
      listItem.selected = true;
      listItem.style.zIndex = length - i;
      container.appendChild(listItem);
    }
    dt.setDragImage(container, 0, 0);

    if (this.canCopyOrDrag_(dt)) {
      if (this.canCutOrDrag_(dt))
        this.cutOrCopy(dt, 'copyMove');
      else
        this.cutOrCopy(dt, 'copy');
    } else {
      event.preventDefault();
    }
  },

  onDragEnd_: function(list, event) {
    var doc = list.ownerDocument;
    var container = doc.querySelector('#drag-image-container');
    container.textContent = '';
    this.setDropTarget_(null);
  },

  onDrag_: function(vent) {
    event.preventDefault();
    event.dataTransfer.dropEffect = 'copy';
  },

  onDragOver_: function(list, event) {
    if (this.canPasteOrDrop_(event.dataTransfer)) {
      event.preventDefault();
    }
  },

  onDragEnter_: function(list, event) {
    var item = list.getListItemAncestor(event.target);
    this.dragEnterCount_++;

    this.setDropTarget_(item && list.isItem(item) ? item : null,
                        event.dataTransfer);
  },

  onDragLeave_: function(event) {
    if (this.dragEnterCount_-- == 0)
      this.setDropTarget_(null);
  },

  onDrop_: function(list, event) {
    console.log('drop: ', event.dataTransfer.dropEffect);
    var item = list.getListItemAncestor(event.target);
    var dropTarget = item && list.isItem(item) ?
       this.fileList_.item(item.listIndex) : null;
    if (dropTarget && !dropTarget.isDirectory)
      dropTarget = null;
    if (!this.canPasteOrDrop_(event.dataTransfer, dropTarget))
      return;
    event.preventDefault();
    this.paste(event.dataTransfer, dropTarget);
    if (this.dropTarget_) {
      var target = this.dropTarget_;
      this.setDropTarget_(null);
    }
  },

  setDropTarget_: function(listItem, opt_dataTransfer) {
    if (this.dropTarget_ == listItem)
      return;

    if (listItem) {
      var entry = this.fileList_.item(listItem.listIndex);
      if (entry.isDirectory &&
          (!opt_dataTransfer ||
           this.canPasteOrDrop_(opt_dataTransfer, entry))) {
        listItem.classList.add('accepts');
      }
    } else {
      this.dragEnterCount_ = 0;
    }
    if (this.dropTarget_ && this.dropTarget_.classList.contains('accepts')) {
      var oldDropTarget = this.dropTarget_;
      var self = this;
      setTimeout(function() {
        if (oldDropTarget != self.dropTarget_)
          oldDropTarget.classList.remove('accepts');
      }, 0);
    }
    this.dropTarget_ = listItem;
  },

  isDocumentWideEvent_: function(event) {
    return event.target == this.document_.body;
  },

  onCopy_: function(event) {
    if (!this.isDocumentWideEvent_(event) ||
        !this.canCopyOrDrag_()) {
      return;
    }
    event.preventDefault();
    this.cutOrCopy(event.clipboardData, 'copy');
    this.notify_('selection-copied');
  },

  onBeforeCopy_: function(event) {
    if (!this.isDocumentWideEvent_(event))
      return;

    // queryCommandEnabled returns true if event.returnValue is false.
    event.returnValue = !this.canCopyOrDrag_();
  },

  canCopyOrDrag_: function() {
    return this.selectedEntries_.length > 0;
  },

  onCut_: function(event) {
    if (!this.isDocumentWideEvent_(event) ||
        !this.canCutOrDrag_()) {
      return;
    }
    event.preventDefault();
    this.cutOrCopy(event.clipboardData, 'move');
    this.notify_('selection-cut');
  },

  onBeforeCut_: function(event) {
    if (!this.isDocumentWideEvent_(event))
      return;
    // queryCommandEnabled returns true if event.returnValue is false.
    event.returnValue = !this.canCutOrDrag_();
  },

  canCutOrDrag_: function() {
    return !this.readonly && this.canCopyOrDrag_();
  },

  onPaste_: function(event) {
    // Need to update here since 'beforepaste' doesn't fire.
    if (!this.isDocumentWideEvent_(event) ||
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

  onBeforePaste_: function(event) {
    if (!this.isDocumentWideEvent_(event))
      return;
    // queryCommandEnabled returns true if event.returnValue is false.
    event.returnValue = !this.canPasteOrDrop_(event.clipboardData);
  },

  canPasteOrDrop_: function(dataTransfer, opt_entry) {
    if (this.readonly)
      return false;  // assure destination entry is in the current directory.

    if (!dataTransfer.types || dataTransfer.types.indexOf('fs/tag') == -1)
      return false;  // Unsupported type of content.
    if (dataTransfer.getData('fs/tag') == '') {
      // Data protected. Other checks are not possible but it makes sense to
      // let the user try.
      return true;
    }

    var directories = dataTransfer.getData('fs/directories').split('\n').
                      filter(function(d) { return d != ''; });

    var destinationPath = (opt_entry || this.currentDirectory).
                          fullPath;
    for (var i = 0; i < directories.length; i++) {
      if (destinationPath.substr(0, directories[i].length) == directories[i])
        return false;  // recursive paste.
    }

    return true;
  },

  queryPasteCommandEnabled: function() {
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
   * @param {string} command 'copy', 'cut' or 'paste'.
   * @param {Function} handler Event handler.
   */
  simulateCommand_: function(command, handler) {
    var iframe = this.document_.querySelector('#command-dispatcher');
    var doc = iframe.contentDocument;
    doc.addEventListener(command, handler);
    doc.execCommand(command);
    doc.removeEventListener(command, handler);
  },

  onSelectionChanged_: function(event) {
    var entries = this.selectedEntries_;
    var dragNodes = this.dragNodes_ = [];
    var files = this.files_ = [];

    for (var i = 0; i < entries.length; i++) {
      // File object must be prepeared in advance for clipboard operations
      // (copy, paste and drag). Clipboard object closes for write after
      // returning control from that handlers so they may not have
      // asynchronous operations.
      if (!this.isOnGData && entries[i].isFile)
        entries[i].file(function(file) { files.push(file); });

      // Items to drag are created in advance. Images must be loaded
      // at the time the 'dragstart' event comes. Otherwise draggable
      // image will be rendered without IMG tags.
      if (dragNodes.length < MAX_DRAG_THUMBAIL_COUNT)
        dragNodes.push(new this.dragNodeConstructor_(entries[i]));
    }
  },

  get currentDirectory() {
    return this.directoryModel_.getCurrentDirEntry();
  },

  get readonly() {
    return this.directoryModel_.isReadOnly();
  },

  get isOnGData() {
    return this.directoryModel_.getRootType() == DirectoryModel.RootType.GDATA;
  },

  notify_: function(eventName) {
    var self = this;
    // Set timeout to avoid recursive events.
    setTimeout(function() {
      cr.dispatchSimpleEvent(self, eventName);
    }, 0);
  },

  /**
   * @type {Array.<Entry>}
   */
  get selectedEntries_() {
    var list = this.fileList_;
    return this.fileListSelection_.selectedIndexes.map(function(index) {
      return list.item(index);
    });
  }
};

