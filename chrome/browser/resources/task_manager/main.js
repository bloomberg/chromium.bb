// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @constructor */
function TaskManager() { }

cr.addSingletonGetter(TaskManager);

TaskManager.prototype = {
  /**
   * Handle window close.
   * @this
   */
  onClose: function() {
    if (!this.disabled_) {
      this.disabled_ = true;
      commands.disableTaskManager();
    }
  },

  /**
   * Handles selection changes.
   * This is also called when data of tasks are refreshed, even if selection
   * has not been changed.
   * @this
   */
  onSelectionChange: function() {
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
        $('kill-process').removeAttribute('disabled');
      else
        $('kill-process').setAttribute('disabled', 'true');

      this.is_end_process_enabled_ = is_end_process_enabled;
    }
  },

  /**
   * Closes taskmanager dialog.
   * After this function is called, onClose() will be called.
   * @this
   */
  close: function() {
    window.close();
  },

  /**
   * Sends commands to kill selected processes.
   * @this
   */
  killSelectedProcesses: function() {
    var selectedIndexes = this.selectionModel_.selectedIndexes;
    var dm = this.dataModel_;
    var uniqueIds = [];
    for (var i = 0; i < selectedIndexes.length; i++) {
      var index = selectedIndexes[i];
      var task = dm.item(index);
      uniqueIds.push(task['uniqueId'][0]);
    }

    commands.killSelectedProcesses(uniqueIds);
  },

  /**
   * Initializes taskmanager.
   * @this
   */
  initialize: function(dialogDom, opt) {
    if (!dialogDom) {
      console.log('ERROR: dialogDom is not defined.');
      return;
    }

    measureTime.startInterval('Load.DOM');

    this.opt_ = opt;

    this.initialized_ = true;

    this.elementsCache_ = {};
    this.dialogDom_ = dialogDom;
    this.document_ = dialogDom.ownerDocument;

    this.localized_column_ = [];
    for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
      var column_label_id = DEFAULT_COLUMNS[i][1];
      this.localized_column_[i] = loadTimeData.getString(column_label_id);
    }

    this.initElements_();
    this.initColumnModel_();
    this.selectionModel_ = new cr.ui.ListSelectionModel();
    this.dataModel_ = new cr.ui.ArrayDataModel([]);

    this.selectionModel_.addEventListener('change',
                                          this.onSelectionChange.bind(this));

    // Initializes compare functions for column sort.
    var dm = this.dataModel_;
    // List of columns to sort by its numerical value as opposed to the
    // formatted value, e.g., 20480 vs. 20KB.
    var COLUMNS_SORTED_BY_VALUE = [
        'cpuUsage', 'physicalMemory', 'sharedMemory', 'privateMemory',
        'networkUsage', 'webCoreImageCacheSize', 'webCoreScriptsCacheSize',
        'webCoreCSSCacheSize', 'fps', 'sqliteMemoryUsed', 'goatsTeleported',
        'v8MemoryAllocatedSize'];

    for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
      var columnId = DEFAULT_COLUMNS[i][0];
      var compareFunc = (function() {
          var columnIdToSort = columnId;
          if (COLUMNS_SORTED_BY_VALUE.indexOf(columnId) != -1)
            columnIdToSort += 'Value';

          return function(a, b) {
              var aValues = a[columnIdToSort];
              var bValues = b[columnIdToSort];
              var aValue = aValues && aValues[0] || 0;
              var bvalue = bValues && bValues[0] || 0;
              return dm.defaultValuesCompareFunction(aValue, bvalue);
          };
      })();
      dm.setCompareFunction(columnId, compareFunc);
    }

    if (isColumnEnabled(DEFAULT_SORT_COLUMN))
      dm.sort(DEFAULT_SORT_COLUMN, DEFAULT_SORT_DIRECTION);

    this.initTable_();

    commands.enableTaskManager();

    // Populate the static localized strings.
    i18nTemplate.process(this.document_, loadTimeData);

    measureTime.recordInterval('Load.DOM');
    measureTime.recordInterval('Load.Total');

    loadDelayedIncludes(this);
  },

  /**
   * Initializes the visibilities and handlers of the elements.
   * This method is called by initialize().
   * @private
   * @this
   */
  initElements_: function() {
    // <if expr="pp_ifdef('chromeos')">
    // The 'close-window' element exists only on ChromeOS.
    // This <if ... /if> section is removed while flattening HTML if chrome is
    // built as Desktop Chrome.
    if (!this.opt_['isShowCloseButton'])
      $('close-window').style.display = 'none';
    $('close-window').addEventListener('click', this.close.bind(this));
    // </if>

    $('kill-process').addEventListener('click',
                                       this.killSelectedProcesses.bind(this));
    $('about-memory-link').addEventListener('click', commands.openAboutMemory);
  },

  /**
   * Additional initialization of taskmanager. This function is called when
   * the loading of delayed scripts finished.
   * @this
   */
  delayedInitialize: function() {
    this.initColumnMenu_();
    this.initTableMenu_();

    var dm = this.dataModel_;
    for (var i = 0; i < dm.length; i++) {
      var processId = dm.item(i)['processId'][0];
      for (var j = 0; j < DEFAULT_COLUMNS.length; j++) {
        var columnId = DEFAULT_COLUMNS[j][0];

        var row = dm.item(i)[columnId];
        if (!row)
          continue;

        for (var k = 0; k < row.length; k++) {
          var labelId = 'detail-' + columnId + '-pid' + processId + '-' + k;
          var label = $(labelId);

          // Initialize a context-menu, if the label exists and its context-
          // menu is not initialized yet.
          if (label && !label.contextMenu)
            cr.ui.contextMenuHandler.setContextMenu(label,
                                                    this.tableContextMenu_);
        }
      }
    }

    this.isFinishedInitDelayed_ = true;
    this.table_.redraw();
  },

  initColumnModel_: function() {
    var table_columns = new Array();
    for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
      var column = DEFAULT_COLUMNS[i];
      var columnId = column[0];
      if (!isColumnEnabled(columnId))
        continue;

      table_columns.push(new cr.ui.table.TableColumn(columnId,
                                                     this.localized_column_[i],
                                                     column[2]));
    }

    for (var i = 0; i < table_columns.length; i++) {
      table_columns[i].renderFunction = this.renderColumn_.bind(this);
    }

    this.columnModel_ = new cr.ui.table.TableColumnModel(table_columns);
  },

  initColumnMenu_: function() {
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
      if (isColumnEnabled(column[0]))
        item.setAttributeNode(this.document_.createAttribute('checked'));
      this.columnSelectContextMenu_.appendChild(item);
    }

    this.document_.body.appendChild(this.columnSelectContextMenu_);
    cr.ui.Menu.decorate(this.columnSelectContextMenu_);

    cr.ui.contextMenuHandler.setContextMenu(this.table_.header,
                                            this.columnSelectContextMenu_);
    cr.ui.contextMenuHandler.setContextMenu(this.table_.list,
                                            this.columnSelectContextMenu_);

    this.document_.addEventListener('command', this.onCommand_.bind(this));
    this.document_.addEventListener('canExecute',
                                    this.onCommandCanExecute_.bind(this));
  },

  initTableMenu_: function() {
    this.table_menu_commands_ = [];
    this.tableContextMenu_ = this.document_.createElement('menu');

    var addMenuItem = function(tm, command_id, string_id) {
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
      item.textContent = loadTimeData.getString(string_id);
      tm.tableContextMenu_.appendChild(item);
    };

    addMenuItem(this, 'inspect', 'inspect');
    addMenuItem(this, 'activate', 'activate');

    this.document_.body.appendChild(this.tableContextMenu_);
    cr.ui.Menu.decorate(this.tableContextMenu_);
  },

  initTable_: function() {
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
    this.table_.fixedHeight = false;

    this.table_.list.addEventListener('contextmenu',
                                      this.onTableContextMenuOpened_.bind(this),
                                      true);

    // Sets custom row render function.
    this.table_.setRenderFunction(this.getRow_.bind(this));
  },

  /**
   * Returns a list item element of the list. This method trys to reuse the
   * cached element, or creates a new element.
   * @return {cr.ui.ListItem}  list item element which contains the given data.
   * @private
   * @this
   */
  getRow_: function(data, table) {
    // Trys to reuse the cached row;
    var listItemElement = this.renderRowFromCache_(data, table);
    if (listItemElement)
      return listItemElement;

    // Initializes the cache.
    var pid = data['processId'][0];
    this.elementsCache_[pid] = {
      listItem: null,
      cell: [],
      icon: [],
      columns: {}
    };

    // Create new row.
    return this.renderRow_(data, table);
  },

  /**
   * Returns a list item element with re-using the previous cached element, or
   * returns null if failed.
   * @return {cr.ui.ListItem} cached un-used element to be reused.
   * @private
   * @this
   */
  renderRowFromCache_: function(data, table) {
    var pid = data['processId'][0];

    // Checks whether the cache exists or not.
    var cache = this.elementsCache_[pid];
    if (!cache)
      return null;

    var listItemElement = cache.listItem;
    var cm = table.columnModel;
    // Checks whether the number of columns has been changed or not.
    if (cache.cachedColumnSize != cm.size)
      return null;
    // Checks whether the number of childlen tasks has been changed or not.
    if (cache.cachedChildSize != data['uniqueId'].length)
      return null;

    // Updates informations of the task if necessary.
    for (var i = 0; i < cm.size; i++) {
      var columnId = cm.getId(i);
      var columnData = data[columnId];
      var oldColumnData = listItemElement.data[columnId];
      var columnElements = cache.columns[columnId];

      if (!columnData || !oldColumnData || !columnElements)
        return null;

      // Sets new width of the cell.
      var cellElement = cache.cell[i];
      cellElement.style.width = cm.getWidth(i) + '%';

      for (var j = 0; j < columnData.length; j++) {
        // Sets the new text, if the text has been changed.
        if (oldColumnData[j] != columnData[j]) {
          var textElement = columnElements[j];
          textElement.textContent = columnData[j];
        }
      }
    }

    // Updates icon of the task if necessary.
    var oldIcons = listItemElement.data['icon'];
    var newIcons = data['icon'];
    if (oldIcons && newIcons) {
      for (var j = 0; j < columnData.length; j++) {
        var oldIcon = oldIcons[j];
        var newIcon = newIcons[j];
        if (oldIcon != newIcon) {
          var iconElement = cache.icon[j];
          iconElement.src = newIcon;
        }
      }
    }
    listItemElement.data = data;

    // Removes 'selected' and 'lead' attributes.
    listItemElement.removeAttribute('selected');
    listItemElement.removeAttribute('lead');

    return listItemElement;
  },

  /**
   * Create a new list item element.
   * @return {cr.ui.ListItem} created new list item element.
   * @private
   * @this
   */
  renderRow_: function(data, table) {
    var pid = data['processId'][0];
    var cm = table.columnModel;
    var listItem = new cr.ui.ListItem({label: ''});

    listItem.className = 'table-row';
    if (this.opt_.isBackgroundMode && data.isBackgroundResource)
      listItem.className += ' table-background-row';

    for (var i = 0; i < cm.size; i++) {
      var cell = document.createElement('div');
      cell.style.width = cm.getWidth(i) + '%';
      cell.className = 'table-row-cell';
      cell.id = 'column-' + pid + '-' + cm.getId(i);
      cell.appendChild(
          cm.getRenderFunction(i).call(null, data, cm.getId(i), table));

      listItem.appendChild(cell);

      // Stores the cell element to the dictionary.
      this.elementsCache_[pid].cell[i] = cell;
    }

    // Specifies the height of the row. The height of each row is
    // 'num_of_tasks * HEIGHT_OF_TASK' px.
    listItem.style.height = (data['uniqueId'].length * HEIGHT_OF_TASK) + 'px';

    listItem.data = data;

    // Stores the list item element, the number of columns and the number of
    // childlen.
    this.elementsCache_[pid].listItem = listItem;
    this.elementsCache_[pid].cachedColumnSize = cm.size;
    this.elementsCache_[pid].cachedChildSize = data['uniqueId'].length;

    return listItem;
  },

  /**
   * Create a new element of the cell.
   * @return {HTMLDIVElement} created cell
   * @private
   * @this
   */
  renderColumn_: function(entry, columnId, table) {
    var container = this.document_.createElement('div');
    container.className = 'detail-container-' + columnId;
    var pid = entry['processId'][0];

    var cache = [];
    var cacheIcon = [];

    if (entry && entry[columnId]) {
      container.id = 'detail-container-' + columnId + '-pid' + entry.processId;

      for (var i = 0; i < entry[columnId].length; i++) {
        var label = document.createElement('div');
        if (columnId == 'title') {
          // Creates a page title element with icon.
          var image = this.document_.createElement('img');
          image.className = 'detail-title-image';
          image.src = entry['icon'][i];
          image.id = 'detail-title-icon-pid' + pid + '-' + i;
          label.appendChild(image);
          var text = this.document_.createElement('div');
          text.className = 'detail-title-text';
          text.id = 'detail-title-text-pid' + pid + '-' + i;
          text.textContent = entry['title'][i];
          label.appendChild(text);

          // Chech if the delayed scripts (included in includes.js) have been
          // loaded or not. If the delayed scripts ware not loaded yet, a
          // context menu could not be initialized. In such case, it will be
          // initialized at delayedInitialize() just after loading of delayed
          // scripts instead of here.
          if (this.isFinishedInitDelayed_)
            cr.ui.contextMenuHandler.setContextMenu(label,
                                                    this.tableContextMenu_);

          label.addEventListener('dblclick', (function(uniqueId) {
              commands.activatePage(uniqueId);
          }).bind(this, entry['uniqueId'][i]));

          label.data = entry;
          label.index_in_group = i;

          cache[i] = text;
          cacheIcon[i] = image;
        } else {
          label.textContent = entry[columnId][i];
          cache[i] = label;
        }
        label.id = 'detail-' + columnId + '-pid' + pid + '-' + i;
        label.className = 'detail-' + columnId + ' pid' + pid;
        container.appendChild(label);
      }

      this.elementsCache_[pid].columns[columnId] = cache;
      if (columnId == 'title')
        this.elementsCache_[pid].icon = cacheIcon;
    }
    return container;
  },

  /**
   * Updates the task list with the supplied task.
   * @private
   * @this
   */
  processTaskChange: function(task) {
    var dm = this.dataModel_;
    var sm = this.selectionModel_;
    if (!dm || !sm) return;

    this.table_.list.startBatchUpdates();
    sm.beginChange();

    var type = task.type;
    var start = task.start;
    var length = task.length;
    var tasks = task.tasks;

    // We have to store the selected pids and restore them after
    // splice(), because it might replace some items but the replaced
    // items would lose the selection.
    var oldSelectedIndexes = sm.selectedIndexes;

    // Create map of selected PIDs.
    var selectedPids = {};
    for (var i = 0; i < oldSelectedIndexes.length; i++) {
      var item = dm.item(oldSelectedIndexes[i]);
      if (item) selectedPids[item['processId'][0]] = true;
    }

    var args = tasks.slice();
    args.unshift(start, dm.length);
    dm.splice.apply(dm, args);

    // Create new array of selected indexes from map of old PIDs.
    var newSelectedIndexes = [];
    for (var i = 0; i < dm.length; i++) {
      if (selectedPids[dm.item(i)['processId'][0]])
        newSelectedIndexes.push(i);
    }

    sm.selectedIndexes = newSelectedIndexes;

    var pids = [];
    for (var i = 0; i < dm.length; i++) {
      pids.push(dm.item(i)['processId'][0]);
    }

    // Sweeps unused caches, which elements no longer exist on the list.
    for (var pid in this.elementsCache_) {
      if (pids.indexOf(pid) == -1)
        delete this.elementsCache_[pid];
    }

    sm.endChange();
    this.table_.list.endBatchUpdates();
  },

  /**
   * Respond to a command being executed.
   * @this
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
        commands.inspect(target_unique_id);
      else if (sub_command == 'activate')
        commands.activatePage(target_unique_id);

      this.currentContextMenuTarget_ = undefined;
    }
  },

  onCommandCanExecute_: function(event) {
    event.canExecute = true;
  },

  /**
   * Store resourceIndex of target resource of context menu, because resource
   * will be replaced when it is refreshed.
   * @this
   */
  onTableContextMenuOpened_: function(e) {
    if (!this.isFinishedInitDelayed_)
      return;

    var mc = this.table_menu_commands_;
    var inspect_menuitem =
        mc[COMMAND_CONTEXTMENU_TABLE_PREFIX + '-inspect'].menuitem;
    var activate_menuitem =
        mc[COMMAND_CONTEXTMENU_TABLE_PREFIX + '-activate'].menuitem;

    // Disabled by default.
    inspect_menuitem.disabled = true;
    activate_menuitem.disabled = true;

    var target = e.target;
    for (;; target = target.parentNode) {
      if (!target) return;
      var classes = target.classList;
      if (classes &&
          Array.prototype.indexOf.call(classes, 'detail-title') != -1) break;
    }

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

  onColumnContextMenu_: function(columnId, command) {
    var menuitem = command.menuitem;
    var checkedItemCount = 0;
    var checked = isColumnEnabled(columnId);

    // Leaves a item visible when user tries making invisible but it is the
    // last one.
    var enabledColumns = getEnabledColumns();
    for (var id in enabledColumns) {
      if (enabledColumns[id])
        checkedItemCount++;
    }
    if (checkedItemCount == 1 && checked)
      return;

    // Toggles the visibility of the column.
    var newChecked = !checked;
    menuitem.checked = newChecked;
    setColumnEnabled(columnId, newChecked);

    this.initColumnModel_();
    this.table_.columnModel = this.columnModel_;
    this.table_.redraw();
  },
};

// |taskmanager| has been declared in preload.js.
taskmanager = TaskManager.getInstance();

function init() {
  var params = parseQueryParams(window.location);
  var opt = {};
  opt['isBackgroundMode'] = params.background;
  opt['isShowCloseButton'] = params.showclose;
  taskmanager.initialize(document.body, opt);
}

document.addEventListener('DOMContentLoaded', init);
document.addEventListener('Close', taskmanager.onClose.bind(taskmanager));
