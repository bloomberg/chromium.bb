// Javascript that is needed for HTML files with the HTML5 media player.
// It does the following:
// * Parses query strings and sets the HTML tag.
// * Installs event handlers to change the HTML title.

var player = null;
function InstallEventHandler(event, action) {
  player.addEventListener(event, function(e) {
    eval(action);
  }, false);
}

var qs = new Array();

function defined(item) {
  return typeof item != 'undefined';
}

function queryString(key) {
  if (!defined(qs[key])) {
    var reQS = new RegExp('[?&]' + key + '=([^&$]*)', 'i');
    var offset = location.search.search(reQS);
    qs[key] = (offset >= 0)? RegExp.$1 : null;
  }
  return qs[key];
}

var media_url = queryString('media');
var ok = true;

if (!queryString('media')) {
  document.title = 'FAIL';
  ok = false;
}

if (defined(queryString('t'))) {
  // Append another parameter "t=" in url that disables media cache.
  media_url += '?t=' + (new Date()).getTime();
}

var tag = queryString('tag');

if (!queryString('tag')) {
  // Default tag is video tag.
  tag = 'video';
}

if (tag != 'audio' && tag != 'video') {
  document.title = 'FAIL';
  ok = false;
}

var container = document.getElementById('player_container');
container.innerHTML = '<' + tag + ' controls id="player"></' + tag + '>';
player = document.getElementById('player');

// Install event handlers.
InstallEventHandler('error',
                    'document.title = "ERROR = " + player.error.code');
InstallEventHandler('playing', 'document.title = "PLAYING"');
InstallEventHandler('ended', 'document.title = "END"');

// Starts the player.
player.src = media_url;