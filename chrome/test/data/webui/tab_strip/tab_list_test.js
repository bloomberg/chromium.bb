// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip/tab_list.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {TabStripEmbedderProxy} from 'chrome://tab-strip/tab_strip_embedder_proxy.js';
import {TabsApiProxy} from 'chrome://tab-strip/tabs_api_proxy.js';

import {TestTabStripEmbedderProxy} from './test_tab_strip_embedder_proxy.js';
import {TestTabsApiProxy} from './test_tabs_api_proxy.js';

class MockDataTransfer extends DataTransfer {
  constructor() {
    super();

    this.dragImageData = {
      image: undefined,
      offsetX: undefined,
      offsetY: undefined,
    };

    this.dropEffect_ = 'none';
    this.effectAllowed_ = 'none';
  }

  get dropEffect() {
    return this.dropEffect_;
  }

  set dropEffect(effect) {
    this.dropEffect_ = effect;
  }

  get effectAllowed() {
    return this.effectAllowed_;
  }

  set effectAllowed(effect) {
    this.effectAllowed_ = effect;
  }

  setDragImage(image, offsetX, offsetY) {
    this.dragImageData.image = image;
    this.dragImageData.offsetX = offsetX;
    this.dragImageData.offsetY = offsetY;
  }
}

suite('TabList', () => {
  let callbackRouter;
  let optionsCalled;
  let tabList;
  let testTabStripEmbedderProxy;
  let testTabsApiProxy;

  const tabs = [
    {
      active: true,
      id: 0,
      index: 0,
      title: 'Tab 1',
    },
    {
      active: false,
      id: 1,
      index: 1,
      title: 'Tab 2',
    },
    {
      active: false,
      id: 2,
      index: 2,
      title: 'Tab 3',
    },
  ];

  function pinTabAt(tab, index) {
    const changeInfo = {index: index, pinned: true};
    const updatedTab = Object.assign({}, tab, changeInfo);
    webUIListenerCallback('tab-updated', updatedTab);
  }

  function getUnpinnedTabs() {
    return tabList.shadowRoot.querySelectorAll('#tabsContainer tabstrip-tab');
  }

  function getPinnedTabs() {
    return tabList.shadowRoot.querySelectorAll(
        '#pinnedTabsContainer tabstrip-tab');
  }

  setup(() => {
    document.body.innerHTML = '';

    testTabsApiProxy = new TestTabsApiProxy();
    testTabsApiProxy.setTabs(tabs);
    TabsApiProxy.instance_ = testTabsApiProxy;
    callbackRouter = testTabsApiProxy.callbackRouter;

    testTabStripEmbedderProxy = new TestTabStripEmbedderProxy();
    testTabStripEmbedderProxy.setColors({
      '--background-color': 'white',
      '--foreground-color': 'black',
    });
    TabStripEmbedderProxy.instance_ = testTabStripEmbedderProxy;

    tabList = document.createElement('tabstrip-tab-list');
    document.body.appendChild(tabList);

    return testTabsApiProxy.whenCalled('getTabs');
  });

  teardown(() => {
    testTabsApiProxy.reset();
    testTabStripEmbedderProxy.reset();
  });

  test('sets theme colors on init', async () => {
    await testTabStripEmbedderProxy.whenCalled('getColors');
    assertEquals(tabList.style.getPropertyValue('--background-color'), 'white');
    assertEquals(tabList.style.getPropertyValue('--foreground-color'), 'black');
  });

  test('updates theme colors when theme changes', async () => {
    testTabStripEmbedderProxy.setColors({
      '--background-color': 'pink',
      '--foreground-color': 'blue',
    });
    webUIListenerCallback('theme-changed');
    await testTabStripEmbedderProxy.whenCalled('getColors');
    assertEquals(tabList.style.getPropertyValue('--background-color'), 'pink');
    assertEquals(tabList.style.getPropertyValue('--foreground-color'), 'blue');
  });

  test('creates a tab element for each tab', () => {
    const tabElements = getUnpinnedTabs();
    assertEquals(tabs.length, tabElements.length);
    tabs.forEach((tab, index) => {
      assertEquals(tabElements[index].tab, tab);
    });
  });

  test('adds a new tab element when a tab is added in same window', () => {
    const appendedTab = {
      id: 3,
      index: 3,
      title: 'New tab',
    };
    webUIListenerCallback('tab-created', appendedTab);
    let tabElements = getUnpinnedTabs();
    assertEquals(tabs.length + 1, tabElements.length);
    assertEquals(tabElements[tabs.length].tab, appendedTab);

    const prependedTab = {
      id: 4,
      index: 0,
      title: 'New tab',
    };
    webUIListenerCallback('tab-created', prependedTab);
    tabElements = getUnpinnedTabs();
    assertEquals(tabs.length + 2, tabElements.length);
    assertEquals(tabElements[0].tab, prependedTab);
  });

  test('adds a new tab element to the start when it is active', async () => {
    const newActiveTab = {
      active: true,
      id: 3,
      index: 3,
      title: 'New tab',
    };
    webUIListenerCallback('tab-created', newActiveTab);
    const [tabId, newIndex] = await testTabsApiProxy.whenCalled('moveTab');
    assertEquals(tabId, newActiveTab.id);
    assertEquals(newIndex, 0);
  });

  test('removes a tab when tab is removed from current window', async () => {
    const tabToRemove = tabs[0];
    webUIListenerCallback('tab-removed', tabToRemove.id);
    await tabList.animationPromises;
    assertEquals(tabs.length - 1, getUnpinnedTabs().length);
  });

  test('updates a tab with new tab data when a tab is updated', () => {
    const tabToUpdate = tabs[0];
    const changeInfo = {title: 'A new title'};
    const updatedTab = Object.assign({}, tabToUpdate, changeInfo);
    webUIListenerCallback('tab-updated', updatedTab);
    const tabElements = getUnpinnedTabs();
    assertEquals(tabElements[0].tab, updatedTab);
  });

  test('updates tabs when a new tab is activated', () => {
    const tabElements = getUnpinnedTabs();

    // Mock activating the 2nd tab
    webUIListenerCallback('tab-active-changed', tabs[1].id);
    assertFalse(tabElements[0].tab.active);
    assertTrue(tabElements[1].tab.active);
    assertFalse(tabElements[2].tab.active);
  });

  test('adds a pinned tab to its designated container', () => {
    webUIListenerCallback('tab-created', {
      index: 0,
      title: 'New pinned tab',
      pinned: true,
    });
    const pinnedTabElements = getPinnedTabs();
    assertEquals(pinnedTabElements.length, 1);
    assertTrue(pinnedTabElements[0].tab.pinned);
  });

  test('moves pinned tabs to designated containers', () => {
    const tabToPin = tabs[1];
    const changeInfo = {index: 0, pinned: true};
    let updatedTab = Object.assign({}, tabToPin, changeInfo);
    webUIListenerCallback('tab-updated', updatedTab);

    let pinnedTabElements = getPinnedTabs();
    assertEquals(pinnedTabElements.length, 1);
    assertTrue(pinnedTabElements[0].tab.pinned);
    assertEquals(pinnedTabElements[0].tab.id, tabToPin.id);
    assertEquals(getUnpinnedTabs().length, 2);

    // Unpin the tab so that it's now at index 0
    changeInfo.index = 0;
    changeInfo.pinned = false;
    updatedTab = Object.assign({}, updatedTab, changeInfo);
    webUIListenerCallback('tab-updated', updatedTab);

    const unpinnedTabElements = getUnpinnedTabs();
    assertEquals(getPinnedTabs().length, 0);
    assertEquals(unpinnedTabElements.length, 3);
    assertEquals(unpinnedTabElements[0].tab.id, tabToPin.id);
  });

  test('moves tab elements when tabs move', () => {
    const tabElementsBeforeMove = getUnpinnedTabs();
    const tabToMove = tabs[0];
    webUIListenerCallback('tab-moved', tabToMove.id, 2);

    const tabElementsAfterMove = getUnpinnedTabs();
    assertEquals(tabElementsBeforeMove[0], tabElementsAfterMove[2]);
    assertEquals(tabElementsBeforeMove[1], tabElementsAfterMove[0]);
    assertEquals(tabElementsBeforeMove[2], tabElementsAfterMove[1]);
  });

  test('activating a tab off-screen scrolls to it', async () => {
    testTabStripEmbedderProxy.setVisible(true);

    const scrollPadding = 32;

    // Mock the width of each tab element
    const tabElements = getUnpinnedTabs();
    tabElements.forEach((tabElement) => {
      tabElement.style.width = '200px';
    });

    // Mock the scrolling parent such that it cannot fit only 1 tab at a time
    const fakeScroller = {
      offsetWidth: 300,
      scrollLeft: 0,
    };
    tabList.scrollingParent_ = fakeScroller;

    // The 2nd tab should be off-screen to the right, so activating it should
    // scroll so that the element's right edge is aligned with the screen's
    // right edge
    webUIListenerCallback('tab-active-changed', tabs[1].id);
    let activeTab = getUnpinnedTabs()[1];
    await tabList.animationPromises;
    assertEquals(
        fakeScroller.scrollLeft,
        activeTab.offsetLeft + activeTab.offsetWidth -
            fakeScroller.offsetWidth + scrollPadding);

    // The 1st tab should be now off-screen to the left, so activating it should
    // scroll so that the element's left edge is aligned with the screen's
    // left edge
    webUIListenerCallback('tab-active-changed', tabs[0].id);
    activeTab = getUnpinnedTabs()[0];
    await tabList.animationPromises;
    assertEquals(fakeScroller.scrollLeft, activeTab.offsetLeft - scrollPadding);
  });

  test('dragstart sets a drag image offset by the event coordinates', () => {
    // Drag and drop only works for pinned tabs
    tabs.forEach(pinTabAt);

    const draggedTab = getPinnedTabs()[0];
    const mockDataTransfer = new MockDataTransfer();
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedTab.dispatchEvent(dragStartEvent);
    assertEquals(dragStartEvent.dataTransfer.effectAllowed, 'move');
    assertEquals(
        mockDataTransfer.dragImageData.image, draggedTab.getDragImage());
    assertEquals(
        mockDataTransfer.dragImageData.offsetX, 100 - draggedTab.offsetLeft);
    assertEquals(
        mockDataTransfer.dragImageData.offsetY, 150 - draggedTab.offsetTop);
  });

  test('dragover moves tabs', async () => {
    // Drag and drop only works for pinned tabs
    tabs.forEach(pinTabAt);

    const draggedIndex = 0;
    const dragOverIndex = 1;
    const draggedTab = getPinnedTabs()[draggedIndex];
    const dragOverTab = getPinnedTabs()[dragOverIndex];
    const mockDataTransfer = new MockDataTransfer();

    // Dispatch a dragstart event to start the drag process
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedTab.dispatchEvent(dragStartEvent);

    // Move the draggedTab over the 2nd tab
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverTab.dispatchEvent(dragOverEvent);
    assertEquals(dragOverEvent.dataTransfer.dropEffect, 'move');
    const [tabId, newIndex] = await testTabsApiProxy.whenCalled('moveTab');
    assertEquals(tabId, tabs[draggedIndex].id);
    assertEquals(newIndex, dragOverIndex);
  });

  test(
      'when the tab strip closes, the active tab should move to the start',
      async () => {
        // Mock activating the 2nd tab
        webUIListenerCallback('tab-active-changed', tabs[1].id);
        testTabsApiProxy.resetResolver('moveTab');

        // Mock tab strip going from visible to hidden
        testTabStripEmbedderProxy.setVisible(false);
        document.dispatchEvent(new Event('visibilitychange'));

        const [moveId, newIndex] = await testTabsApiProxy.whenCalled('moveTab');
        assertEquals(moveId, tabs[1].id);
        assertEquals(newIndex, 0);
      });
});
