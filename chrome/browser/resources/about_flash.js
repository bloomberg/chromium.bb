/**
* Takes the |moduleListData| input argument which represents data about
* the currently available modules and populates the html jstemplate
* with that data. It expects an object structure like the above.
* @param {Object} moduleListData Information about available modules
*/
function renderTemplate(moduleListData) {
  // This is the javascript code that processes the template:
  var input = new JsEvalContext(moduleListData);
  var output = document.getElementById('flashInfoTemplate');
  jstProcess(input, output);
}

/**
* Asks the C++ FlashUIDOMHandler to get details about the Flash and return
* the data in returnFlashInfo() (below).
*/
function requestFlashInfo() {
  chrome.send('requestFlashInfo', []);
}

/**
* Called by the WebUI to re-populate the page with data representing the
* current state of Flash.
*/
function returnFlashInfo(moduleListData) {
  document.getElementById('loading-message').style.visibility = 'hidden';
  document.getElementById('body-container').style.visibility = 'visible';
  renderTemplate(moduleListData);
}

// Get data and have it displayed upon loading.
document.addEventListener('DOMContentLoaded', requestFlashInfo);

