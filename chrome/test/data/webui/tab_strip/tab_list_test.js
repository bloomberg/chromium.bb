// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip/tab_list.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {TabsApiProxy} from 'chrome://tab-strip/tabs_api_proxy.js';
import {ThemeProxy} from 'chrome://tab-strip/theme_proxy.js';

import {TestTabsApiProxy} from './test_tabs_api_proxy.js';
import {TestThemeProxy} from './test_theme_proxy.js';

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
  let testTabsApiProxy;
  let testThemeProxy;

  const currentWindow = {
    id: 1001,
    tabs: [
      {
        active: true,
        id: 0,
        index: 0,
        title: 'Tab 1',
        windowId: 1001,
      },
      {
        active: false,
        id: 1,
        index: 1,
        title: 'Tab 2',
        windowId: 1001,
      },
      {
        active: false,
        id: 2,
        index: 2,
        title: 'Tab 3',
        windowId: 1001,
      },
    ],
  };

  function pinTabAt(tab, index) {
    const changeInfo = {index: index, pinned: true};
    const updatedTab = Object.assign({}, tab, changeInfo);
    callbackRouter.onUpdated.dispatchEvent(tab.id, changeInfo, updatedTab);
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
    testTabsApiProxy.setCurrentWindow(currentWindow);
    TabsApiProxy.instance_ = testTabsApiProxy;

    testThemeProxy = new TestThemeProxy();
    testThemeProxy.setColors({
      '--background-color': 'white',
      '--foreground-color': 'black',
    });
    ThemeProxy.instance_ = testThemeProxy;

    callbackRouter = testTabsApiProxy.callbackRouter;

    tabList = document.createElement('tabstrip-tab-list');
    document.body.appendChild(tabList);

    return testTabsApiProxy.whenCalled('getCurrentWindow');
  });

  test('sets theme colors on init', async () => {
    await testThemeProxy.whenCalled('getColors');
    assertEquals(tabList.style.getPropertyValue('--background-color'), 'white');
    assertEquals(tabList.style.getPropertyValue('--foreground-color'), 'black');
  });

  test('updates theme colors when theme changes', async () => {
    testThemeProxy.resetResolver('getColors');
    testThemeProxy.setColors({
      '--background-color': 'pink',
      '--foreground-color': 'blue',
    });
    webUIListenerCallback('theme-changed');
    await testThemeProxy.whenCalled('getColors');
    assertEquals(tabList.style.getPropertyValue('--background-color'), 'pink');
    assertEquals(tabList.style.getPropertyValue('--foreground-color'), 'blue');
  });

  test('creates a tab element for each tab', () => {
    const tabElements = getUnpinnedTabs();
    assertEquals(currentWindow.tabs.length, tabElements.length);
    currentWindow.tabs.forEach((tab, index) => {
      assertEquals(tabElements[index].tab, tab);
    });
  });

  test('adds a new tab element when a tab is added in same window', () => {
    const appendedTab = {
      id: 3,
      index: 3,
      title: 'New tab',
      windowId: currentWindow.id,
    };
    callbackRouter.onCreated.dispatchEvent(appendedTab);
    let tabElements = getUnpinnedTabs();
    assertEquals(currentWindow.tabs.length + 1, tabElements.length);
    assertEquals(tabElements[currentWindow.tabs.length].tab, appendedTab);

    const prependedTab = {
      id: 4,
      index: 0,
      title: 'New tab',
      windowId: currentWindow.id,
    };
    callbackRouter.onCreated.dispatchEvent(prependedTab);
    tabElements = getUnpinnedTabs();
    assertEquals(currentWindow.tabs.length + 2, tabElements.length);
    assertEquals(tabElements[0].tab, prependedTab);
  });

  test(
      'does not add a new tab element when a tab is added in a different ' +
          'window',
      () => {
        const newTab = {
          index: 3,
          title: 'New tab',
          windowId: currentWindow.id + 1,
        };
        callbackRouter.onCreated.dispatchEvent(newTab);
        const tabElements = getUnpinnedTabs();
        assertEquals(currentWindow.tabs.length, tabElements.length);
      });

  test('removes a tab when tab is removed from current window', async () => {
    const tabToRemove = currentWindow.tabs[0];
    callbackRouter.onRemoved.dispatchEvent(tabToRemove.id, {
      windowId: currentWindow.id,
    });
    await tabList.animationPromises;
    assertEquals(currentWindow.tabs.length - 1, getUnpinnedTabs().length);
  });

  test('updates a tab with new tab data when a tab is updated', () => {
    const tabToUpdate = currentWindow.tabs[0];
    const changeInfo = {title: 'A new title'};
    const updatedTab = Object.assign({}, tabToUpdate, changeInfo);
    callbackRouter.onUpdated.dispatchEvent(
        tabToUpdate.id, changeInfo, updatedTab);

    const tabElements = getUnpinnedTabs();
    assertEquals(tabElements[0].tab, updatedTab);
  });

  test('updates tabs when a new tab is activated', () => {
    const tabElements = getUnpinnedTabs();

    // Mock activating the 2nd tab
    callbackRouter.onActivated.dispatchEvent({
      tabId: currentWindow.tabs[1].id,
      windowId: currentWindow.id,
    });
    assertFalse(tabElements[0].tab.active);
    assertTrue(tabElements[1].tab.active);
    assertFalse(tabElements[2].tab.active);
  });

  test('adds a pinned tab to its designated container', () => {
    callbackRouter.onCreated.dispatchEvent({
      index: 0,
      title: 'New pinned tab',
      pinned: true,
      windowId: currentWindow.id,
    });
    const pinnedTabElements = getPinnedTabs();
    assertEquals(pinnedTabElements.length, 1);
    assertTrue(pinnedTabElements[0].tab.pinned);
  });

  test('moves pinned tabs to designated containers', () => {
    const tabToPin = currentWindow.tabs[1];
    const changeInfo = {index: 0, pinned: true};
    let updatedTab = Object.assign({}, tabToPin, changeInfo);
    callbackRouter.onUpdated.dispatchEvent(tabToPin.id, changeInfo, updatedTab);
    let pinnedTabElements = getPinnedTabs();
    assertEquals(pinnedTabElements.length, 1);
    assertTrue(pinnedTabElements[0].tab.pinned);
    assertEquals(pinnedTabElements[0].tab.id, tabToPin.id);
    assertEquals(getUnpinnedTabs().length, 2);

    // Unpin the tab so that it's now at index 0
    changeInfo.index = 0;
    changeInfo.pinned = false;
    updatedTab = Object.assign({}, updatedTab, changeInfo);
    callbackRouter.onUpdated.dispatchEvent(tabToPin.id, changeInfo, updatedTab);
    const unpinnedTabElements = getUnpinnedTabs();
    assertEquals(getPinnedTabs().length, 0);
    assertEquals(unpinnedTabElements.length, 3);
    assertEquals(unpinnedTabElements[0].tab.id, tabToPin.id);
  });

  test('moves tab elements when tabs move', () => {
    const tabElementsBeforeMove = getUnpinnedTabs();
    const tabToMove = currentWindow.tabs[0];
    callbackRouter.onMoved.dispatchEvent(tabToMove.id, {
      toIndex: 2,
      windowId: currentWindow.id,
    });
    const tabElementsAfterMove = getUnpinnedTabs();
    assertEquals(tabElementsBeforeMove[0], tabElementsAfterMove[2]);
    assertEquals(tabElementsBeforeMove[1], tabElementsAfterMove[0]);
    assertEquals(tabElementsBeforeMove[2], tabElementsAfterMove[1]);
  });

  test('activating a tab off-screen scrolls to it', async () => {
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
    callbackRouter.onActivated.dispatchEvent({
      tabId: currentWindow.tabs[1].id,
      windowId: currentWindow.id,
    });
    let activeTab = getUnpinnedTabs()[1];
    await tabList.animationPromises;
    assertEquals(
        fakeScroller.scrollLeft,
        activeTab.offsetLeft + activeTab.offsetWidth -
            fakeScroller.offsetWidth + scrollPadding);

    // The 1st tab should be now off-screen to the left, so activating it should
    // scroll so that the element's left edge is aligned with the screen's
    // left edge
    callbackRouter.onActivated.dispatchEvent({
      tabId: currentWindow.tabs[0].id,
      windowId: currentWindow.id,
    });
    activeTab = getUnpinnedTabs()[0];
    await tabList.animationPromises;
    assertEquals(fakeScroller.scrollLeft, activeTab.offsetLeft - scrollPadding);
  });

  test('attaching a tab creates a new tab element', async () => {
    const attachedTab = {
      index: 2,
      id: 9001,
      title: 'My new tab',
      windowId: currentWindow.id,
    };
    testTabsApiProxy.setTab(attachedTab);
    callbackRouter.onAttached.dispatchEvent(attachedTab.id, {
      newPosition: attachedTab.index,
      newWindowId: attachedTab.windowId,
    });

    const tabId = await testTabsApiProxy.whenCalled('getTab');
    assertEquals(tabId, attachedTab.id);
    assertEquals(getUnpinnedTabs().length, currentWindow.tabs.length + 1);
    assertEquals(getUnpinnedTabs()[attachedTab.index].tab, attachedTab);
  });

  test('detaching a tab removes the tab element', () => {
    const detachedTab = currentWindow.tabs[1];
    callbackRouter.onDetached.dispatchEvent(detachedTab.id, {
      oldPosition: 1,
      oldWindowId: currentWindow.id,
    });
    assertEquals(getUnpinnedTabs().length, currentWindow.tabs.length - 1);
  });

  test(
      'respects the last attached/detached event when multiple events are ' +
          'dispatched for the same tab',
      async () => {
        const attachedTab = {
          index: 2,
          id: 9001,
          title: 'My new tab',
          windowId: currentWindow.id,
        };
        testTabsApiProxy.setTab(attachedTab);
        callbackRouter.onAttached.dispatchEvent(attachedTab.id, {
          newPosition: attachedTab.index,
          newWindowId: attachedTab.windowId,
        });
        callbackRouter.onDetached.dispatchEvent(attachedTab.id, {
          oldPosition: attachedTab.index,
          oldWindowId: attachedTab.windowId,
        });
        callbackRouter.onAttached.dispatchEvent(attachedTab.id, {
          newPosition: attachedTab.index,
          newWindowId: attachedTab.windowId,
        });
        callbackRouter.onDetached.dispatchEvent(attachedTab.id, {
          oldPosition: attachedTab.index,
          oldWindowId: attachedTab.windowId,
        });
        await testTabsApiProxy.whenCalled('getTab');
        assertEquals(getUnpinnedTabs().length, currentWindow.tabs.length);
      });

  test('dragstart sets a drag image offset by the event coordinates', () => {
    const draggedTab = getUnpinnedTabs()[0];
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
    const draggedIndex = 0;
    const dragOverIndex = 1;
    const draggedTab = getUnpinnedTabs()[draggedIndex];
    const dragOverTab = getUnpinnedTabs()[dragOverIndex];
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
    assertEquals(tabId, currentWindow.tabs[draggedIndex].id);
    assertEquals(newIndex, dragOverIndex);
  });
});
