// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @constructor */
function TaskManager() { }

cr.addSingletonGetter(TaskManager);

/*
 * Default columns (column_id, label_id, width, is_default)
 * @const
 */
var DEFAULT_COLUMNS = [
    ['title', 'pageColumn', 300, true],
    ['profileName', 'profileNameColumn', 120, false],
    ['physicalMemory', 'physicalMemColumn', 80, true],
    ['sharedMemory', 'sharedMemColumn', 80, false],
    ['privateMemory', 'privateMemColumn', 80, false],
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

var COMMAND_CONTEXTMENU_COLUMN_PREFIX = 'columnContextMenu';
var COMMAND_CONTEXTMENU_TABLE_PREFIX = 'tableContextMenu';

var localStrings = new LocalStrings();

TaskManager.prototype = {
  /**
   * Handle window close.
   * @public
   */
  onClose: function () {
    if (!this.disabled_) {
      this.disabled_ = true;
      this.disableTaskManager();
    }
  },

  /**
   * Handle selection change.
   * This is also called when data of tasks are refleshed, even if selection
   * has not been changed.
   * @public
   */
  onSelectionChange: function () {
    var sm = this.selectionModel_;
    var dm = this.dataModel_;
    var selectedIndexes = sm.selectedIndexes;
    var is_end_process_enabled = true;
    if (selectedIndexes.length == 0)
      is_end_process_enabled = false;
    for (var i = 0; i < selectedIndexes.length; i++) {
      var index = selectedIndexes[i];
      var task = dm.item(index);
      if (task['type'] == 'BROWSER')
        is_end_process_enabled = false;
    }
    if (this.is_end_process_enabled_ != is_end_process_enabled) {
      if (is_end_process_enabled)
        $('kill-process').removeAttribute("disabled");
      else
        $('kill-process').setAttribute("disabled", "true");

      this.is_end_process_enabled_ = is_end_process_enabled;
    }
  },

  /**
   * Closes taskmanager dialog.
   * After this function is called, onClose() will be called.
   * @public
   */
  close: function () {
    window.close();
  },

  /**
   * Sends commands to kill a process.
   * @public
   */
  killProcess: function () {
    var selectedIndexes = this.selectionModel_.selectedIndexes;
    chrome.send('killProcess', selectedIndexes);
  },

  /**
   * Sends command to initiate resource inspection.
   * @public
   */
  inspect: function (uniqueId) {
    chrome.send('inspect', [uniqueId]);
  },

  /**
   * Sends command to kill a process.
   * @public
   */
  openAboutMemory: function () {
    chrome.send('openAboutMemory');
  },

  /**
   * Sends command to disable taskmanager model.
   * @public
   */
  disableTaskManager: function () {
    chrome.send('disableTaskManager');
  },

  /**
   * Sends command to enable taskmanager model.
   * @public
   */
  enableTaskManager: function () {
    chrome.send('enableTaskManager');
  },

  /**
   * Sends command to activate a page.
   * @public
   */
  activatePage: function (uniqueId) {
    chrome.send('activatePage', [uniqueId]);
  },

  /**
   * Initializes taskmanager.
   * @public
   */
  initialize: function (dialogDom, opt) {
    if (!dialogDom) {
      console.log('ERROR: dialogDom is not defined.');
      return;
    }

    this.opt_ = opt;

    this.initialized_ = true;
    this.enableTaskManager();

    this.dialogDom_ = dialogDom;
    this.document_ = dialogDom.ownerDocument;

    $('close-window').addEventListener('click', this.close.bind(this));
    $('kill-process').addEventListener('click', this.killProcess.bind(this));
    $('about-memory-link').addEventListener('click',
                                            this.openAboutMemory.bind(this));

    this.is_column_shown_ = [];
    for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
      this.is_column_shown_[i] = DEFAULT_COLUMNS[i][3];
    }

    this.localized_column_ = [];
    for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
      var column_label_id = DEFAULT_COLUMNS[i][1];
      var localized_label = localStrings.getString(column_label_id);
      // Falls back to raw column_label_id if localized string is not defined.
      if (localized_label == "")
        localized_label = column_label_id;

      this.localized_column_[i] = localized_label;
    }

    this.initColumnModel_();
    this.selectionModel_ = new cr.ui.ListSelectionModel();
    this.dataModel_ = new cr.ui.ArrayDataModel([]);

    this.selectionModel_.addEventListener('change',
                                          this.onSelectionChange.bind(this));

    // Initializes compare functions for column sort.
    var dm = this.dataModel_;
    // Columns to sort by value instead of itself.
    var columns_sorted_by_value = [
        'cpuUsage', 'physicalMemory', 'sharedMemory', 'privateMemory',
        'networkUsage', 'webCoreImageCacheSize', 'webCoreScriptsCacheSize',
        'webCoreCSSCacheSize', 'fps', 'sqliteMemoryUsed', 'goatsTeleported',
        'v8MemoryAllocatedSize'];

    for (var i in columns_sorted_by_value) {
      var column_id = columns_sorted_by_value[i];
      var compare_func = function() {
          var value_id = column_id + 'Value';
          return function(a, b) {
              var aValues = a[value_id];
              var bValues = b[value_id];
              var aValue = aValues && aValues[0] || 0;
              var bvalue = bValues && bValues[0] || 0;
              return dm.defaultValuesCompareFunction(aValue, bvalue);
          };
      }();
      dm.setCompareFunction(column_id, compare_func);
    }

    var ary = this.dialogDom_.querySelectorAll('[visibleif]');
    for (var i = 0; i < ary.length; i++) {
      var expr = ary[i].getAttribute('visibleif');
      if (!eval(expr))
        ary[i].hidden = true;
    }

    this.initTable_();
    this.initColumnMenu_();
    this.initTableMenu_();
    this.table_.redraw();
  },

  initColumnModel_: function () {
    var table_columns = new Array();
    for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
      if (!this.is_column_shown_[i])
        continue;

      var column = DEFAULT_COLUMNS[i];
      table_columns.push(new cr.ui.table.TableColumn(column[0],
                                                     this.localized_column_[i],
                                                     column[2]));
    }

    for (var i = 0; i < table_columns.length; i++) {
      table_columns[i].renderFunction = this.renderColumn_.bind(this);
    }

    this.columnModel_ = new cr.ui.table.TableColumnModel(table_columns);
  },

  initColumnMenu_: function () {
    this.column_menu_commands_ = [];

    this.commandsElement_ = this.document_.createElement('commands');
    this.document_.body.appendChild(this.commandsElement_);

    this.columnSelectContextMenu_ = this.document_.createElement('menu');
    for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
      var column = DEFAULT_COLUMNS[i];

      // Creates command element to receive event.
      var command = this.document_.createElement('command');
      command.id = COMMAND_CONTEXTMENU_COLUMN_PREFIX + '-' + column[0];
      cr.ui.Command.decorate(command);
      this.column_menu_commands_[command.id] = command;
      this.commandsElement_.appendChild(command);

      // Creates menuitem element.
      var item = this.document_.createElement('menuitem');
      item.command = command;
      command.menuitem = item;
      item.textContent = this.localized_column_[i];
      if (this.is_column_shown_[i])
        item.setAttributeNode(this.document_.createAttribute("checked"));
      this.columnSelectContextMenu_.appendChild(item);
    }

    this.document_.body.appendChild(this.columnSelectContextMenu_);
    cr.ui.Menu.decorate(this.columnSelectContextMenu_);

    cr.ui.contextMenuHandler.addContextMenuProperty(this.table_.header);
    this.table_.header.contextMenu = this.columnSelectContextMenu_;

    cr.ui.contextMenuHandler.addContextMenuProperty(this.table_.list);
    this.table_.list.contextMenu = this.columnSelectContextMenu_;

    this.document_.addEventListener('command', this.onCommand_.bind(this));
    this.document_.addEventListener('canExecute',
                                    this.onCommandCanExecute_.bind(this));

  },

  initTableMenu_: function () {
    this.table_menu_commands_ = [];
    this.tableContextMenu_ = this.document_.createElement('menu');

    var addMenuItem = function (tm, command_id, string_id, default_label) {
      // Creates command element to receive event.
      var command = tm.document_.createElement('command');
      command.id = COMMAND_CONTEXTMENU_TABLE_PREFIX + '-' + command_id;
      cr.ui.Command.decorate(command);
      tm.table_menu_commands_[command.id] = command;
      tm.commandsElement_.appendChild(command);

      // Creates menuitem element.
      var item = tm.document_.createElement('menuitem');
      item.command = command;
      command.menuitem = item;
      var localized_label = localStrings.getString(string_id);
      item.textContent = localized_label || default_label;
      tm.tableContextMenu_.appendChild(item);
    };

    addMenuItem(this, 'inspect', 'inspect', "Inspect");
    addMenuItem(this, 'activate', 'activate', "Activate");

    this.document_.body.appendChild(this.tableContextMenu_);
    cr.ui.Menu.decorate(this.tableContextMenu_);
  },

  initTable_: function () {
    if (!this.dataModel_ || !this.selectionModel_ || !this.columnModel_) {
      console.log('ERROR: some models are not defined.');
      return;
    }

    this.table_ = this.dialogDom_.querySelector('.detail-table');
    cr.ui.Table.decorate(this.table_);

    this.table_.dataModel = this.dataModel_;
    this.table_.selectionModel = this.selectionModel_;
    this.table_.columnModel = this.columnModel_;

    // Expands height of row when a process has some tasks.
    this.table_.autoExpands = true;

    this.table_.list.addEventListener('contextmenu',
                                      this.onTableContextMenuOpened_.bind(this),
                                      true);

    // Sets custom row render function.
    this.table_.setRenderFunction(this.renderRow_.bind(this));
  },

  renderRow_: function(dataItem, table) {
    var cm = table.columnModel;
    var listItem = new cr.ui.ListItem({label: ''});

    listItem.className = 'table-row';
    if (this.opt_.isBackgroundMode && dataItem.isBackgroundResource)
      listItem.className += ' table-background-row';

    for (var i = 0; i < cm.size; i++) {
      var cell = document.createElement('div');
      cell.style.width = cm.getWidth(i) + '%';
      cell.className = 'table-row-cell';
      cell.appendChild(
          cm.getRenderFunction(i).call(null, dataItem, cm.getId(i), table));

      listItem.appendChild(cell);
    }
    listItem.data = dataItem;

    return listItem;
  },

  renderColumn_: function(entry, columnId, table) {
    var container = this.document_.createElement('div');
    container.id = 'detail-container-' + columnId + '-pid' + entry.processId;
    container.className = 'detail-container-' + columnId;

    if (entry[columnId]) {
      for (var i = 0; i < entry[columnId].length; i++) {
        var label = document.createElement('div');
        if (columnId == 'title') {
          var image = this.document_.createElement('img');
          image.className = 'detail-title-image';
          image.src = entry['icon'][i];
          label.appendChild(image);
          var text = this.document_.createElement('div');
          text.className = 'detail-title-text';
          text.textContent = entry['title'][i];
          label.appendChild(text);

          cr.ui.contextMenuHandler.addContextMenuProperty(label);
          label.contextMenu = this.tableContextMenu_;

          label.addEventListener('dblclick', (function(uniqueId) {
              this.activatePage(uniqueId);
          }).bind(this, entry['uniqueId'][i]));

          label.data = entry;
          label.index_in_group = i;
        } else {
          label.textContent = entry[columnId][i];
        }
        label.id = 'detail-' + columnId + '-pid' + entry.processId + '-' + i;
        label.className = 'detail-' + columnId + ' pid' + entry.processId;
        container.appendChild(label);
      }
    }
    return container;
  },

  onTaskChange: function (start, length, tasks) {
    var dm = this.dataModel_;
    var sm = this.selectionModel_;
    if (!dm || !sm)
      return;

    // Splice takes the to-be-spliced-in array as individual parameters,
    // rather than as an array, so we need to perform some acrobatics...
    var args = [].slice.call(tasks);
    args.unshift(start, length);

    var oldSelectedIndexes = sm.selectedIndexes;
    dm.splice.apply(dm, args);
    sm.selectedIndexes = oldSelectedIndexes;
  },

  onTaskAdd: function (start, length, tasks) {
    var dm = this.dataModel_;
    if (!dm)
      return;

    // Splice takes the to-be-spliced-in array as individual parameters,
    // rather than as an array, so we need to perform some acrobatics...
    var args = [].slice.call(tasks);
    args.unshift(start, 0);

    dm.splice.apply(dm, args);
  },

  onTaskRemove: function (start, length, tasks) {
    var dm = this.dataModel_;
    if (!dm)
      return;
    dm.splice(start, length);
  },

  /**
   * Respond to a command being executed.
   */
  onCommand_: function(event) {
    var command = event.command;
    var command_id = command.id.split('-', 2);

    var main_command = command_id[0];
    var sub_command = command_id[1];

    if (main_command == COMMAND_CONTEXTMENU_COLUMN_PREFIX) {
      this.onColumnContextMenu_(sub_command, command);
    } else if (main_command == COMMAND_CONTEXTMENU_TABLE_PREFIX) {
      var target_unique_id = this.currentContextMenuTarget_;

      if (!target_unique_id)
        return;

      if (sub_command == 'inspect')
        this.inspect(target_unique_id);
      else if (sub_command == 'activate')
        this.activatePage(target_unique_id);

      this.currentContextMenuTarget_ = undefined;
    }
  },

  onCommandCanExecute_: function(event) {
    event.canExecute = true;
  },

  /**
   * Store resourceIndex of target resource of context menu, because resource
   * will be replaced when it is refleshed.
   */
  onTableContextMenuOpened_: function (e) {
    var mc = this.table_menu_commands_;
    var inspect_menuitem =
        mc[COMMAND_CONTEXTMENU_TABLE_PREFIX + '-inspect'].menuitem;
    var activate_menuitem =
        mc[COMMAND_CONTEXTMENU_TABLE_PREFIX + '-activate'].menuitem;

    // Disabled by default.
    inspect_menuitem.disabled = true;
    activate_menuitem.disabled = true;

    var target = e.target;
    var classes = target.classList;
    while (target &&
        Array.prototype.indexOf.call(classes, 'detail-title') == -1) {
      target = target.parentNode;
      classes = target.classList;
    }

    if (!target)
      return;

    var index_in_group = target.index_in_group;

    // Sets the uniqueId for current target page under the mouse corsor.
    this.currentContextMenuTarget_ = target.data['uniqueId'][index_in_group];

    // Enables if the page can be inspected.
    if (target.data['canInspect'][index_in_group])
      inspect_menuitem.disabled = false;

    // Enables if the page can be activated.
    if (target.data['canActivate'][index_in_group])
      activate_menuitem.disabled = false;
  },

  onColumnContextMenu_: function(id, command) {
    var menuitem = command.menuitem;
    var checked_item_count = 0;
    var is_uncheck = 0;

    // Leaves a item visible when user tries making invisible but it is the
    // last one.
    for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
      var column = DEFAULT_COLUMNS[i];
      if (column[0] == id && this.is_column_shown_[i]) {
        is_uncheck = 1;
      }
      if (this.is_column_shown_[i])
        checked_item_count++;
    }
    if (checked_item_count == 1 && is_uncheck) {
      return;
    }

    // Toggles the visibility of the column.
    for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
      var column = DEFAULT_COLUMNS[i];
      if (column[0] == id) {
        this.is_column_shown_[i] = !this.is_column_shown_[i];
        menuitem.checked = this.is_column_shown_[i];
        this.initColumnModel_()
        this.table_.columnModel = this.columnModel_;
        this.table_.redraw();
        return;
      }
    }
  },
};

var taskmanager = TaskManager.getInstance();

function init() {
  var params = parseQueryParams(window.location);
  var opt = {};
  opt['isShowTitle'] = params.showtitle;
  opt['isBackgroundMode'] = params.background;
  opt['isShowCloseButton'] = params.showclose;
  taskmanager.initialize(document.body, opt);
}

function onClose() {
  return taskmanager.onClose();
}

document.addEventListener('DOMContentLoaded', init);
document.addEventListener('Close', onClose);

function taskAdded(start, length, tasks) {
  // Sometimes this can get called too early.
  if (!taskmanager)
    return;
  taskmanager.onTaskAdd(start, length, tasks);
}

function taskChanged(start, length,tasks) {
  // Sometimes this can get called too early.
  if (!taskmanager)
    return;
  taskmanager.onTaskChange(start, length, tasks);
}

function taskRemoved(start, length) {
  // Sometimes this can get called too early.
  if (!taskmanager)
    return;
  taskmanager.onTaskRemove(start, length);
}

