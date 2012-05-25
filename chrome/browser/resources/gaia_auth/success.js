function load() {
  console.log('### Authenticator success page loaded.');

  var params = getUrlSearchParams(location.search);
  var msg = {
    'method': 'confirmLogin',
    'attemptToken': params['attemptToken']
  };
  window.parent.postMessage(msg,
          'chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/main.html');
}

document.addEventListener('DOMContentLoaded', load);

