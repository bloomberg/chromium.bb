// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {

  function displayUserErrorMessage(message) {
    chrome.notifications.create('no_current_tab', {
      type: 'basic',
      title: 'Error!',
      message: message,
      iconUrl: '../icons/ic_error_outline_black_2x_web_24dp.png'
    }, null);
  }

  // IndexedDB implementation of the recipe object.
  // We use IndexedDB to implement the recipe object because:
  //    * IndexedDB is persistent.
  //    * IndexedDB access are atomic and synchronized.
  //
  // The Action Recorder Extension uses non-persistent background scripts.
  // The Extension loads the background scripts to respond to events, and
  // unload the background scripts when event handler functions finish.
  // JavaScript variables do not persist between background script sessions,
  // so the extension cannot store the recipe object as a local variable.
  //
  // The background scripts must synchronize access to the recipe object
  // to ensure that the extension records user actions in chronological
  // order. Often, the extension catches multiple user events in a short
  // period of time. If the extension does not synchronize access to the
  // recipe object, event handlers in the background script will run
  // into race conditions that break the recipe.

  function openRecipeIndexedDB() {
    return new Promise((resolve, reject) => {
      const request = window.indexedDB.open(Indexed_DB_Vars.RECIPE_DB,
                                            Indexed_DB_Vars.VERSION);
      request.onerror = (event) => {
        console.warn('Unable to load the indexedDB Recipe database!');
        reject(request.error);
      };
      request.onsuccess = (event) => {
        const db = request.result;
        resolve(db);
      };
      request.onupgradeneeded = (event) => {
        const db = request.result;
        db.onerror = (event) => {
          console.warn('Unable to load the indexedDB Recipe database!');
          reject(event);
        };
        db.createObjectStore(Indexed_DB_Vars.ATTRIBUTES,
                             { autoIncrement: true });
        db.createObjectStore(Indexed_DB_Vars.ACTIONS,
                             { keyPath: 'action_index', autoIncrement: true });
        event.target.transaction.oncomplete = (event) => {
          resolve(db);
        };
      };
    })
    .catch((error) => {
      console.error('Unable to open the indexedDB Recipe database!', error);
    });
  }

  function clearRecipeIndexedDB() {
    return new Promise((resolve, reject) => {
      const request =
          window.indexedDB.deleteDatabase(Indexed_DB_Vars.RECIPE_DB);
      request.onerror = (event) => {
        console.warn('Error deleting indexedDB database.');
        reject(request.error);
      };
      request.onsuccess = (event) => {
        console.log('IndexedDB database deleted successfully.');
        resolve();
      };
    });
  }

  async function performTransactionOnRecipeIndexedDB(transactionToPerform) {
    const db = await openRecipeIndexedDB();
    return await new Promise((resolve, reject) => {
      const transaction = db.transaction([Indexed_DB_Vars.ATTRIBUTES,
                                          Indexed_DB_Vars.ACTIONS],
                                         'readwrite');
      transaction.oncomplete = (event) => {
        resolve(event);
      };
      transaction.onerror = (event) => {
        console.warn('Unable to complete indexedDB transaction.');
        reject(transaction.error);
      };
      transactionToPerform(transaction);
    })
    .catch((error) => {
      console.error('Unable to perform transaction in the indexedDB ' +
                    'Recipe database!', error);
    })
    .finally(() => {
      db.close();
    });
  }

  function initializeRecipe(name, url) {
    return performTransactionOnRecipeIndexedDB((transaction) => {
        const attributeStore =
            transaction.objectStore(Indexed_DB_Vars.ATTRIBUTES);
        attributeStore.put(name, Indexed_DB_Vars.NAME);
        attributeStore.put(url, Indexed_DB_Vars.URL);
      });
  }

  async function addActionToRecipe(action, tabId) {
    const db = await openRecipeIndexedDB();
    const key = await new Promise((resolve, reject) => {
      const transaction = db.transaction([Indexed_DB_Vars.ACTIONS],
                                         'readwrite');
      const request =
          transaction.objectStore(Indexed_DB_Vars.ACTIONS).add(action);
      let completed = false;
      let key;
      request.onsuccess = (event) => {
        key = event.target.result;
        if (completed) {
          resolve(key);
        }
      };
      transaction.oncomplete = (event) => {
        completed = true;
        if (key) {
          resolve(key);
        }
      };
      transaction.onerror = (event) => {
        console.warn('Unable to add action to the recipe!');
        reject(event.target.error);
      };
    })
    .finally(() => {
      db.close();
    });

    // Update the recording UI with the new action.
    const frameId = await getRecorderUiFrameId();
    action.action_index = key;
    await sendMessageToTab(
        tabId,
        { type: RecorderUiMsgEnum.ADD_ACTION, action: action},
        { frameId: frameId });
  }

  function removeActionFromRecipe(index) {
    return performTransactionOnRecipeIndexedDB((transaction) => {
      transaction.objectStore(Indexed_DB_Vars.ACTIONS).delete(index);
    });
  }

  async function getRecipe() {
    const db = await openRecipeIndexedDB();
    let recipe = {};
    return await new Promise((resolve, reject) => {
      let recipe = {};
      const transaction = db.transaction([Indexed_DB_Vars.ATTRIBUTES,
                                          Indexed_DB_Vars.ACTIONS],
                                         'readonly');
      transaction.oncomplete = (event) => {
        resolve(recipe);
      };
      transaction.onerror = (event) => {
        console.error('Unable to read from indexedDB.');
        throw(transaction.error);
      };
      const attributeStore =
        transaction.objectStore(Indexed_DB_Vars.ATTRIBUTES);
      const actionsStore = transaction.objectStore(Indexed_DB_Vars.ACTIONS);
      const nameReq = attributeStore.get(Indexed_DB_Vars.NAME);
      nameReq.onsuccess = (event) => {
        recipe.name = nameReq.result;
      };
      const urlReq = attributeStore.get(Indexed_DB_Vars.URL);
      urlReq.onsuccess = (event) => {
        recipe.startingURL = urlReq.result;
      };
      const actionsReq = actionsStore.getAll();
      actionsReq.onsuccess = (event) => {
        recipe.actions = actionsReq.result ? actionsReq.result : [];
      };
    })
    .finally(() => {
      db.close();
    });
  }

  function setBrowserActionUi(state, targetTabId) {
    switch(state) {
      case RecorderStateEnum.SHOWN:
        chrome.browserAction.setTitle(
            {title: `Action Recorder (Recording on tab ${targetTabId})`});
        chrome.browserAction.setBadgeText({ text: 'HIDE' });
        chrome.browserAction.setBadgeBackgroundColor(
            {color: [255, 0, 0, 255]});
        break;
      case RecorderStateEnum.HIDDEN:
        chrome.browserAction.setTitle({
            title: `Action Recorder (Recording on tab ${targetTabId}),`
                 + ' UI is hidden'});
        chrome.browserAction.setBadgeText({ text: 'SHOW' });
        chrome.browserAction.setBadgeBackgroundColor(
            {color: [0, 255, 0, 255]});
        break;
      default:
        chrome.browserAction.setTitle({title: 'Action Recorder (Idle)'});
        chrome.browserAction.setBadgeText({ text: 'START' });
        chrome.browserAction.setBadgeBackgroundColor(
            {color: [0, 255, 255, 255]});
        break;
    }
  }

  async function getRecordingTabId() {
    const items = await getChromeLocalStorageVariables(
        [Local_Storage_Vars.RECORDING_TAB_ID]);
    return items[Local_Storage_Vars.RECORDING_TAB_ID];
  }

  async function getRecordingState() {
    const items = await getChromeLocalStorageVariables(
        [Local_Storage_Vars.RECORDING_STATE]);
    return items[Local_Storage_Vars.RECORDING_STATE];
  }

  async function getRecorderUiFrameId() {
    const items = await getChromeLocalStorageVariables(
        [Local_Storage_Vars.RECORDING_UI_FRAME_ID]);
    return items[Local_Storage_Vars.RECORDING_UI_FRAME_ID];
  }

  async function setRecorderUiFrameId(frameId) {
    let items = {};
    items[Local_Storage_Vars.RECORDING_UI_FRAME_ID] = frameId;
    await setChromeLocalStorageVariables(items);
  }

  function sendMessageToTab(tabId, message, options) {
    return new Promise((resolve, reject) => {
        chrome.tabs.sendMessage(tabId, message, options, (response) => {
          resolve(response);
        });
      });
  }

  function getAllFramesInTab(tabId) {
    return new Promise((resolve, reject) => {
        chrome.webNavigation.getAllFrames({tabId: tabId}, (details) => {
          resolve(details);
        });
      });
  }

  function clearBrowserCache() {
    return new Promise((resolve, reject) => {
        chrome.browsingData.removeCache({}, () => {
          if (chrome.runtime.lastError) {
            reject(chrome.runtime.lastError);
          } else {
            resolve();
          }
        })
      });
  }

  async function getIframeContext(tabId, frameId) {
    if (frameId === 0) {
      return { isIframe: false };
    }

    let context = { isIframe: true };

    const allFrames = await getAllFramesInTab(tabId);
    let targetFrame;
    for (const frame of allFrames) {
      if (frame.frameId === frameId) {
        targetFrame = frame;
        break;
      }
    }

    // Send a message to the parent frame and see if the iframe has a
    // 'name' attribute.
    const frameName = await sendMessageToTab(tabId,
       { type: RecorderMsgEnum.GET_IFRAME_NAME,
         url: targetFrame.url},
       { frameId: targetFrame.parentFrameId});
    if (frameName) {
      context.browserTest = { name: frameName };
      return context;
    }

    const targetFrameUrl = new URL(targetFrame.url);
    // The frame does not have a 'name' attribute. Check if the frame has
    // a unique origin.
    //
    // The Captured Site automation framework can identify an iframe by its
    // origin, provided the origin is unique.
    //
    // Identifying an iframe through its origin is more preferable than
    // identifying an iframe through its URL. An URL will frequently
    // contain parameters, and many websites use random number generator or
    // date generator to create these parameters. For example, in the
    // following URL
    //
    // https://payment.bhphotovideo.com/static/desktop/v2.0/
    // index.html
    // #paypageId=aLGNuLSTJVwgEiCn&cartID=333334444
    // &receiverID=77777777-7777-4777-b777-777777888888
    // &uuid=77777777-7777-4777-b777-778888888888
    //
    // The site created the parameters cartID, receiverID and uuid using
    // random number generators. These parameters will have different
    // values every time the browser loads the page. Therefore automation
    // will not be able to identify an iframe that loads this URL.
    let originIsUnique = true;
    for (const frame of allFrames) {
      const url = new URL(frame.url);
      if (frame.frameId !== targetFrame.frameId &&
          url.origin === targetFrameUrl.origin) {
        originIsUnique = false;
        break;
      }
    }

    if (originIsUnique) {
      context.browserTest =
          { origin: `${targetFrameUrl.protocol}//${targetFrameUrl.host}` };
      return context;
    }

    context.browserTest = { url: targetFrame.url };
    return context;
  }

  async function stopRecordingOnTab(tabId) {
    const allFrames = await getAllFramesInTab(tabId);
    let stopRecordingOnMainFrameResponse;

    for (const frame of allFrames) {
      try {
        const response = await sendMessageToTab(
            tabId, { type: RecorderMsgEnum.STOP }, { frameId: frame.frameId });
        if (frame.frameId === 0) {
          stopRecordingOnMainFrameResponse = response;
        }
      } catch (error) {
        console.warn(`Unable to stop recording on '${frame.url}'`, error);
      }
    }

    if (!stopRecordingOnMainFrameResponse) {
      return Promise.reject(
          new Error('Unable to stop recording on the root frame!'));
    }

    const destroyRecordingUiResponse = await sendMessageToTab(
        tabId, { type: RecorderUiMsgEnum.DESTROY_UI }, { frameId: 0 });
    if (!destroyRecordingUiResponse) {
      return Promise.reject(new Error('Unable to destroy the recorder UI!'));
    }
    setBrowserActionUi(RecorderStateEnum.STOPPED);
  }

  async function startRecording(tab) {
    // By default, start recording on the current active tab.
    await clearRecorderVariables();
    await initializeRecorderVariables(tab);
    const allFrames = await getAllFramesInTab(tab.id);
    for (const frame of allFrames) {
      // The extension has no need and no permission to inject script
      // into 'about:' pages, such as the 'about:blank' page.
      if (!frame.url.startsWith('about:')) {
        try {
          const response =
              await startRecordingOnTabAndFrame(tab.id, frame.frameId);
        } catch (error) {
          console.warn(`Unable to start recording for url ${tab.url}!\r\n`,
                       error);
        }
      }
    }
    console.log(`Started recording for tab ${tab.id}`);
  }

  async function startRecordingOnTabAndFrame(tabId, frameId) {
    const context = await getIframeContext(tabId, frameId);
    const response = await sendMessageToTab(
        tabId,
        { type: RecorderMsgEnum.START,
          frameContext: context},
        { frameId: frameId });
    if (!response) {
      return Promise.reject(
          new Error(`Unable to start recording on ${tabId}, ${frameId}`));
    }

    // If starting recording on the root frame, tell the root frame to
    // create the recorder UI.
    if (frameId === 0) {
      const createUiResponse = await sendMessageToTab(
          tabId, { type: RecorderUiMsgEnum.CREATE_UI }, { frameId: 0 });
      if (!response) {
        return Promise.reject(new Error('Unable to create the recorder UI!'));
      }
      let items = {};
      items[Local_Storage_Vars.RECORDING_STATE] = RecorderStateEnum.SHOWN;
      await setChromeLocalStorageVariables(items);
      setBrowserActionUi(RecorderStateEnum.SHOWN, tabId);
    }
  }

  async function initializeRecorderVariables(tab) {
    let items = {};
    // Initialize the target tab id in the local storage area. The
    // background script will use this id to communicate with the content
    // script on the actively recording tab.
    items[Local_Storage_Vars.RECORDING_TAB_ID] = tab.id;
    await setChromeLocalStorageVariables(items);
    await initializeRecipe(tab.title, tab.url);
  }

  // Clean up local storage variables used for recording.
  function clearRecorderVariables() {
    return new Promise((resolve, reject) => {
        chrome.storage.local.clear(() => {
          if (chrome.runtime.lastError) {
            reject(chrome.runtime.lastError);
          } else {
            clearRecipeIndexedDB()
            .then(() => resolve());
          }
        });
      });
  }

  async function stopRecording() {
    const tabId = await getRecordingTabId();
    await stopRecordingOnTab(tabId);
    await clearRecorderVariables();
    await clearBrowserCache();
    console.log('Stopped Recording.')
  }

  async function downloadRecipe() {
    const recipe = await getRecipe();

    // Download the JSON-serialized recipe as a text file.
    const recipeJsonString = JSON.stringify(recipe, null, 2);
    const blob = new Blob([recipeJsonString], {type: 'text/plain'});
    return new Promise((resolve, reject) => {
      chrome.downloads.download({
            filename: '_web_site_name.test',
            saveAs: true,
            url: window.URL.createObjectURL(blob)
          }, (downloadId) => {
        if (downloadId) {
          resolve();
        } else {
          reject('Download failed! Recording isn\'t stopped.');
        }
      });
    });
  }

  async function downloadRecipeAndStopRecording() {
    await downloadRecipe();
    await stopRecording();
  }

  async function showContentScriptUi() {
    const tabId = await getRecordingTabId();
    await sendMessageToTab(tabId, { type: RecorderUiMsgEnum.SHOW_UI },
                           { frameId: 0 });
    let items = {};
    items[Local_Storage_Vars.RECORDING_STATE] = RecorderStateEnum.SHOWN;
    await setChromeLocalStorageVariables(items);
    setBrowserActionUi(RecorderStateEnum.SHOWN, tabId);
  }

  async function hideContentScriptUi() {
    const tabId = await getRecordingTabId();
    await sendMessageToTab(tabId, { type: RecorderUiMsgEnum.HIDE_UI },
                           { frameId: 0 });
    let items = {};
    items[Local_Storage_Vars.RECORDING_STATE] = RecorderStateEnum.HIDDEN;
    await setChromeLocalStorageVariables(items);
    setBrowserActionUi(RecorderStateEnum.HIDDEN, tabId);
  }

  // Reset the action recorder state to a clean slate every time the background
  // script is loaded.
  // Used to reset the recorder state in the scenario that the user previously
  // closed a session without stopping the recorder. Since the recorder uses
  // indexedDB and local storage, the recording state would persist. The
  // browser action icon state also persists.
  clearRecorderVariables()
  .then(() => setBrowserActionUi(RecorderStateEnum.STOPPED))
  .catch((error) => {
    console.error('Unable to clean up the recorder!\r\n', error);
    displayUserErrorMessage('Unable to clean up the recorder!');
  });

  // Reset the action recorder state to a clean slate every time the extension
  // is installed or reinstalled.
  // Used to reset the recorder state when reloading the extension after a code
  // change.
  chrome.runtime.onInstalled.addListener(() => {
    clearRecorderVariables()
    .then(() => setBrowserActionUi(RecorderStateEnum.STOPPED))
    .catch((error) => {
      console.error('Unable to clean up the recorder!\r\n', error);
      displayUserErrorMessage('Unable to clean up the recorder!');
    });
  });

  async function browserActionOnClickedHander(tab) {
    const state = await getRecordingState();
    switch(state) {
      case RecorderStateEnum.SHOWN:
        try {
          await hideContentScriptUi();
        } catch (error) {
          console.error('Unable to hide the recorder UI!\r\n', error);
          displayUserErrorMessage('Unable to hide the recording UI!');
        }
        break;
      case RecorderStateEnum.HIDDEN:
        try {
          showContentScriptUi()
        } catch(error) {
          console.error('Unable to show the recorder UI!\r\n', error);
          displayUserErrorMessage('Unable to show the recording UI!');
        }
        break;
      default:
        // By default, start recording on the current active tab.
        try {
          await startRecording(tab);
        } catch(startRecordingError) {
          console.error('Unable to start recording!\r\n', startRecordingError);
          displayUserErrorMessage('Unable to start recording!');
          try {
            await stopRecording();
          } catch(stopRecordingError) {
            console.warn('Unable to clean up recording!', stopRecordingError);
            displayUserErrorMessage('Unable to clean up recording!');
          }
        }
    }
  }

  chrome.browserAction.onClicked.addListener((tab) => {
    browserActionOnClickedHander(tab);
  });

  async function tabsOnRemovedHandler(tabId) {
    const recordingTabId = await getRecordingTabId();
    if (recordingTabId == tabId) {
      try {
        await downloadRecipe();
        await clearRecorderVariables();
      } catch (error) {
        console.error(
            `Unable to stop recording on the closing tab ${tabId}!\r\n`,
            error);
      } finally {
        setBrowserActionUi(RecorderStateEnum.STOPPED);
      }
    }
  }

  chrome.tabs.onRemoved.addListener((tabId, removeInfo) => {
    tabsOnRemovedHandler(tabId);
  });

  async function webNavigationOnCompletedHander(details) {
    const tabId = await getRecordingTabId();
    if (details.tabId === tabId &&
        // Skip recording on 'about:' pages. No meaningful user interaction
        // occur on 'about:'' pages such as the blank page. Plus, this
        // extension has no permission to access 'about:' pages.
        !details.url.startsWith('about:')) {
      try {
        await startRecordingOnTabAndFrame(tabId, details.frameId);
        console.log(`Resumed recording on tab ${tabId}`);

        if (details.frameId !== 0) {
          return;
        }

        // If the tab's root frame loaded a new page, log the page's url.
        // An engineer can truncate a test recipe generated for captured sites
        // using the 'loadPage' actions. If the Web Page Replay (WPR) tool can
        // serve a 'loadPage' action url, then the engineer can delete all the
        // actions that precedes the 'loadPage' action.
        addActionToRecipe({
          url: details.url,
          context: { 'isIframe': false },
          type: 'loadPage'
        });

        const state = await getRecordingState();
        if (state === RecorderStateEnum.HIDDEN) {
          setBrowserActionUi(RecorderStateEnum.SHOWN, tabId);
        }
      } catch (error) {
        if (details.frameId === 0) {
          console.error('Unable to resume recording!', error);
          await stopRecordingOnTab(tabId);
        } else {
          console.warn('Unable to resume recording on the recording ' +
                       `tab ${tabId}, frame ${details.frameId}, ` +
                       `url '${details.url}'!`, error);
        }
      }
    }
  }

  chrome.webNavigation.onCompleted.addListener((details) => {
    webNavigationOnCompletedHander(details);
  })

  chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    if (!request) return false;
    switch (request.type) {
      case RecorderMsgEnum.SAVE:
        downloadRecipe()
        .then(() => sendResponse(true));
        return true;
      case RecorderMsgEnum.STOP:
        downloadRecipeAndStopRecording()
        .catch((error) => {
          console.error('Unable to stop recording!\r\n', error);
          displayUserErrorMessage('Unable to stop recording!');
        });
        sendResponse(true);
        break;
      case RecorderMsgEnum.CANCEL:
        stopRecording()
        .catch((error) => {
          console.error('Unable to stop recording!\r\n', error);
          displayUserErrorMessage('Unable to stop recording!');
        });
        sendResponse(true);
        break;
      case RecorderMsgEnum.ADD_ACTION:
        addActionToRecipe(request.action, sender.tab.id)
        .then(() => { return sendResponse(true); })
        .catch((error) => {
          console.error(
              `Unable to add the ${request.action.type} action for ` +
              `${request.action.selector}!\r\n`, error);
          displayUserErrorMessage('Unable to record the action!');
          sendResponse(false);
        });
        return true;
      case RecorderUiMsgEnum.REMOVE_ACTION:
        removeActionFromRecipe(request.action_index)
        .then(() => {
          sendResponse(true);
        })
        .catch((error) => {
          sendResponse(false);
          console.error(
              'Unable to remove the action indexed at ' +
              `${request.action_index}!\r\n`,
              error);
          displayUserErrorMessage('Unable to remove action!');
        });
        return true;
      case RecorderUiMsgEnum.GET_RECIPE:
        setRecorderUiFrameId(sender.frameId)
        .then(() => getRecipe())
        .then((recipe) => sendResponse(recipe));
        return true;
      default:
    }
    return false;
  });

})();
