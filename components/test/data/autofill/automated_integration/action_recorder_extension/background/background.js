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

  function performTransactionOnRecipeIndexedDB(transactionToPerform) {
    return openRecipeIndexedDB()
      .then((db) => {
        return new Promise((resolve, reject) => {
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

  function addActionToRecipe(action) {
    return openRecipeIndexedDB()
      .then((db) => {
        return new Promise((resolve, reject) => {
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
      });
  }

  function removeActionFromRecipe(index) {
    return performTransactionOnRecipeIndexedDB((transaction) => {
      transaction.objectStore(Indexed_DB_Vars.ACTIONS).delete(index);
    });
  }

  function getRecipe() {
    return openRecipeIndexedDB()
      .then((db) => {
        return new Promise((resolve, reject) => {
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

  function getRecordingTabId() {
    return new Promise((resolve, reject) => {
        getChromeLocalStorageVariables([Local_Storage_Vars.RECORDING_TAB_ID])
        .then((items) => {
          resolve(items[Local_Storage_Vars.RECORDING_TAB_ID]);
        })
        .catch((message) => {
          reject(message);
        });
      });
  }

  function getRecordingState() {
    return new Promise((resolve, reject) => {
        getChromeLocalStorageVariables([Local_Storage_Vars.RECORDING_STATE])
        .then((items) => {
          resolve(items[Local_Storage_Vars.RECORDING_STATE]);
        })
        .catch((message) => {
          reject(message);
        });
      });
  }

  function getRecorderUiFrameId() {
    return new Promise((resolve, reject) => {
        getChromeLocalStorageVariables(
            [Local_Storage_Vars.RECORDING_UI_FRAME_ID])
        .then((items) => {
          resolve(items[Local_Storage_Vars.RECORDING_UI_FRAME_ID]);
        })
        .catch((message) => {
          reject(message);
        });
      });
  }

  function setRecorderUiFrameId(frameId) {
    let items = {};
    items[Local_Storage_Vars.RECORDING_UI_FRAME_ID] = frameId;
    return setChromeLocalStorageVariables(items);
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

  function getIframeContext(tabId, frameId) {
    return new Promise((resolve, reject) => {
        if (frameId === 0) {
          resolve({ isIframe: false });
        } else {
          let context = { isIframe: true };
          getAllFramesInTab(tabId)
          .then((details) => {
            let targetFrame;
            for (let index = 0; index < details.length; index++) {
              if (details[index].frameId === frameId) {
                targetFrame = details[index];
                break;
              }
            }
            // Send a message to the parent frame and see if the iframe has a
            // 'name' attribute.
            sendMessageToTab(tabId, {
              type: RecorderMsgEnum.GET_IFRAME_NAME,
              url: targetFrame.url
            }, {
              frameId: targetFrame.parentFrameId
            })
            .then((frameName) => {
              if (frameName !== '') {
                context.browserTest = { name: frameName };
                resolve(context);
              } else {
                const targetFrameUrl = new URL(targetFrame.url);
                // The frame does not have a 'name' attribute. Check if the
                // frame has a unique combination of scheme, host and port.
                //
                // The Captured Site automation framework can identify an
                // iframe by its scheme + host + port, provided this
                // information combination is unique. Identifying an iframe
                // through its scheme + host + port is more preferable than
                // identifying an iframe through its URL. An URL will
                // frequently contain parameters, and many websites use random
                // number generator or date generator to create these
                // parameters. For example, in the following URL
                //
                // https://payment.bhphotovideo.com/static/desktop/v2.0/
                // index.html
                // #paypageId=aLGNuLSTJVwgEiCn&cartID=333334444
                // &receiverID=77777777-7777-4777-b777-777777888888
                // &uuid=77777777-7777-4777-b777-778888888888
                //
                // The site created the parameters cartID, receiverID and uuid
                // using random number generators. These parameters will have
                // different values every time the browser loads the page.
                // Therefore automation will not be able to identify an iframe
                // that loads this URL.
                let frameHostAndSchemeIsUnique = true;
                for (let index = 0; index < details.length; index++) {
                  const url = new URL(details[index].url);
                  if (details[index].frameId !== targetFrame.frameId &&
                      targetFrameUrl.protocol === url.protocol &&
                      targetFrameUrl.host === url.host) {
                    frameHostAndSchemeIsUnique = false;
                    break;
                  }
                }
                if (frameHostAndSchemeIsUnique) {
                  context.browserTest = {
                      schemeAndHost:
                          `${targetFrameUrl.protocol}//${targetFrameUrl.host}`
                    };
                  resolve(context);
                } else {
                  context.browserTest = { url: targetFrame.url };
                  resolve(context);
                }
              }
            });
          });
        }
      });
  }

  function stopRecordingOnTab(tabId) {
    const promise =
      // Send a message to all the frames in the target tab to stop recording.
      getAllFramesInTab(tabId)
      .then((details) => {
        let recordingStoppedOnMainFramePromise;
        details.forEach((frame) => {
          const promise = sendMessageToTab(tabId,
                                           { type: RecorderMsgEnum.STOP },
                                           { frameId: frame.frameId });
          if (frame.frameId === 0) {
            recordingStoppedOnMainFramePromise = promise;
          } else {
            promise.catch((error) =>
              console.warn(
                  `Unable to stop recording on '${frame.url}'`, error));
          }
        });
        return recordingStoppedOnMainFramePromise;
      })
      .then((response) => {
        if (!response) {
          return Promise.reject(
              new Error('Unable to stop recording on the root frame!'));
        }
        return sendMessageToTab(
            tabId, { type: RecorderUiMsgEnum.DESTROY_UI }, { frameId: 0 });
      })
      .then((response) => {
        if (!response) {
          return Promise.reject(
              new Error('Unable to destroy the recorder UI!'));
        }
        setBrowserActionUi(RecorderStateEnum.STOPPED);
        return Promise.resolve();
      });
    return promise;
  }

  function startRecordingOnTabAndFrame(tabId, frameId) {
    const ret =
      getIframeContext(tabId, frameId)
      .then((context) => {
        return sendMessageToTab(tabId,
                                { type: RecorderMsgEnum.START,
                                  frameContext: context
                                },
                                { frameId: frameId });
      })
      .then((response) => {
        if (!response) {
          return Promise.reject(
              new Error(`Unable to start recording on ${tabId}, ${frameId}`));
        }
        // If starting recording on the root frame, tell the root frame to
        // create the recorder UI.
        if (frameId === 0) {
          return sendMessageToTab(
              tabId, { type: RecorderUiMsgEnum.CREATE_UI }, { frameId: 0 })
            .then((response) => {
              if (!response) {
                return Promise.reject(
                  new Error('Unable to create the recorder UI!'));
              }
              let items = {};
              items[Local_Storage_Vars.RECORDING_STATE] =
                  RecorderStateEnum.SHOWN;
              return setChromeLocalStorageVariables(items);
            })
            .then(() => {
              setBrowserActionUi(RecorderStateEnum.SHOWN, tabId);
              return Promise.resolve();
            });
        } else {
          return Promise.resolve();
        }
      });
    return ret;
  }

  function initializeRecorderVariables(tab) {
    let items = {};
    // Initialize the target tab id in the local storage area. The
    // background script will use this id to communicate with the content
    // script on the actively recording tab.
    items[Local_Storage_Vars.RECORDING_TAB_ID] = tab.id;
    return setChromeLocalStorageVariables(items)
      .then(initializeRecipe(tab.title, tab.url));
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
            resolve();
          }
        });
      });
  }

  function stopRecording() {
    return getRecordingTabId()
      .then((tabId) => stopRecordingOnTab(tabId))
      .then(() => clearRecorderVariables());
  }

  function downloadRecipe() {
    return new Promise((resolve, reject) => {
        getRecipe()
        .then((recipe) => {
          // Download the JSON-serialized recipe as a text file.
          const recipeJsonString = JSON.stringify(recipe, null, 2);
          const blob = new Blob([recipeJsonString], {type: 'text/plain'});
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
        })
        .catch((message) => {
          reject(message);
        });
      });
  }

  function showContentScriptUi() {
    return new Promise((resolve, reject) => {
        getRecordingTabId()
        .then((tabId) => {
          return sendMessageToTab(tabId, { type: RecorderUiMsgEnum.SHOW_UI },
                                  { frameId: 0 });
        })
        .then(() => {
          let items = {};
          items[Local_Storage_Vars.RECORDING_STATE] = RecorderStateEnum.SHOWN;
          return setChromeLocalStorageVariables(items);
        })
        .then(() => {
          return getRecordingTabId();
        })
        .then((tabId) => {
          setBrowserActionUi(RecorderStateEnum.SHOWN, tabId);
          resolve();
        })
        .catch((message) => {
          reject(message);
        })
      });
  }

  function hideContentScriptUi() {
    return new Promise((resolve, reject) => {
        getRecordingTabId()
        .then((tabId) => {
          return sendMessageToTab(tabId,
                                  { type: RecorderUiMsgEnum.HIDE_UI },
                                  { frameId: 0 });
        })
        .then(() => {
          let items = {};
          items[Local_Storage_Vars.RECORDING_STATE] = RecorderStateEnum.HIDDEN;
          return setChromeLocalStorageVariables(items);
        })
        .then(() => {
          return getRecordingTabId();
        })
        .then((tabId) => {
          setBrowserActionUi(RecorderStateEnum.HIDDEN, tabId);
          resolve();
        })
        .catch((message) => {
          reject(message);
        })
      });
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

  chrome.browserAction.onClicked.addListener((tab) => {
    getRecordingState()
    .then((state) => {
      switch(state) {
        case RecorderStateEnum.SHOWN:
          hideContentScriptUi()
          .catch((error) => {
            console.error('Unable to hide the recorder UI!\r\n', error);
            displayUserErrorMessage('Unable to hide the recording UI!');
          });
          break;
        case RecorderStateEnum.HIDDEN:
          showContentScriptUi()
          .catch((error) => {
            console.error('Unable to show the recorder UI!\r\n', error);
            displayUserErrorMessage('Unable to show the recording UI!');
          });
          break;
        default:
          // By default, start recording on the current active tab.
          clearRecorderVariables()
          .then(() => initializeRecorderVariables(tab))
          .then(() => getAllFramesInTab(tab.id))
          .then((details) => {
            let recordingStartedOnRootFramePromise;
            details.forEach((frame) => {
              // The extension has no need and no permission to inject script
              // into 'about:' pages, such as the 'about:blank' page.
              if (!frame.url.startsWith('about:')) {
                const promise =
                    startRecordingOnTabAndFrame(tab.id, frame.frameId);
                if (frame.frameId === 0) {
                  recordingStartedOnRootFramePromise = promise;
                } else {
                  promise.catch((error) => {
                      console.warn(
                          `Unable to start recording for url ${tab.url}!\r\n`,
                          error);
                  });
                }
              }
            });
            return recordingStartedOnRootFramePromise;
          })
          .then(() => console.log(`Started recording for tab ${tab.id}`))
          .catch((error) => {
            console.error('Unable to start recording!\r\n', error);
            displayUserErrorMessage('Unable to start recording!');
            stopRecording().catch((error) => {
              console.warn('Unable to clean up recording!', error);
              displayUserErrorMessage('Unable to clean up recording!');
            });
          });
          break;
      }
    });
  });

  chrome.tabs.onRemoved.addListener((tabId, removeInfo) => {
    getRecordingTabId()
    .then((recordingTabId) => {
      if (recordingTabId == tabId) {
        downloadRecipe()
        .then(() => clearRecorderVariables())
        .finally(() => setBrowserActionUi(RecorderStateEnum.STOPPED))
        .catch((error) => {
          console.error('Unable to stop recording on the closing tab ' +
                        `${tabId}!\r\n`, error);
        });
      }
    });
  });

  chrome.webNavigation.onCompleted.addListener((details) => {
    getRecordingTabId().then((tabId) => {
      if (details.tabId === tabId &&
          // Skip recording on 'about:' pages. No meaningful user interaction
          // occur on 'about:'' pages such as the blank page. Plus, this
          // extension has no permission to access 'about:' pages.
          !details.url.startsWith('about:')) {
        startRecordingOnTabAndFrame(tabId, details.frameId)
        .then(() => getRecordingState())
        .then((state) => {
          console.log(`Resumed recording on tab ${tabId}`);
          if (state === RecorderStateEnum.HIDDEN) {
            setBrowserActionUi(RecorderStateEnum.SHOWN, tabId);
          }
        })
        .catch((error) => {
          if (details.frameId === 0) {
            console.error('Unable to resume recording!', error);
            stopRecordingOnTab(tabId);
          }
          console.warn('Unable to resume recording on the recording ' +
                       `tab ${tabId}, frame ${details.frameId}, ` +
                       `url '${details.url}'!`, error);
        });
      }
    });
  })

  chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    if (!request) return false;
    switch (request.type) {
      // Tab commands.
      // Query for a frame's frame id and parent frame id.
      case RecorderMsgEnum.GET_FRAME_CONTEXT:
        getIframeContext(sender.tab.id, sender.frameId, request.location)
        .then((context) => {
          sendResponse(context);
        })
        .catch((error) => {
          console.error(
              `Unable to query for context on tab ${sender.tab.id}, ` +
              `frame ${sender.frameId}!\r\n`,
              error);
        });
        return true;
      case RecorderMsgEnum.SAVE:
        downloadRecipe()
        .then(() => sendResponse(true));
        return true;
      case RecorderMsgEnum.STOP:
        downloadRecipe()
        .then(() => {
          return stopRecording();
        })
        .then(() => clearBrowserCache())
        .then(() => console.log('Stopped Recording.'))
        .catch((error) => {
          console.error('Unable to stop recording!\r\n', error);
          displayUserErrorMessage('Unable to stop recording!');
        });
        sendResponse(true);
        break;
      case RecorderMsgEnum.CANCEL:
        stopRecording()
        .then(() => clearBrowserCache())
        .then(()  => console.log('Stopped Recording.'))
        .catch((error) => {
          console.error('Unable to stop recording!\r\n', error);
          displayUserErrorMessage('Unable to stop recording!');
        });
        sendResponse(true);
        break;
      case RecorderMsgEnum.ADD_ACTION:
        addActionToRecipe(request.action)
        .then((key) => {
          sendResponse(true);
          return getRecorderUiFrameId()
            .then((frameId) => {
              request.action.action_index = key;
              return sendMessageToTab(
                  sender.tab.id,
                  { type: RecorderUiMsgEnum.ADD_ACTION,
                    action: request.action}, { frameId: frameId });
            });
        })
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
