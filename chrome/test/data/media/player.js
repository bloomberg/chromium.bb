var player = null;

function GetTestParameters() {
  var parts = window.location.href.split('?');
  if (parts.length != 2)
    return null;
  var query = parts[1];
  var params = new Array();
  var query_parts = query.split('=');
  if (query_parts.length == 2) {
    params['tag'] = query_parts[0];
    params['url'] = query_parts[1];
  } else if (query_parts.length == 1) {
    params['url'] = query_parts[0];
  } else {
    return null;
  }
  return params;
}

function SetupPlayer(tag, fullscreen) {
  var container = document.getElementById('player_container');
  container.innerHTML = '<' + tag + ' controls id="player"></' + tag + '>';
  player = document.getElementById('player');
  player.addEventListener('error',
                          function() {
                            document.title = "ERROR = " + player.error.code;
                          },
                          false);
  if (fullscreen) {
    player.addEventListener('canplay',
                            function() {
                              if (!player.webkitSupportsFullscreen) {
                                document.title = "ERROR fullscreen unsupported";
                              } else {
                                document.title = "READY";
                              }
                            },
                            false);
    document.onclick = function () {
      try {
        player.webkitEnterFullScreen();
      } catch (err) {}
      if (!player.webkitDisplayingFullscreen) {
        document.title = "ERROR entering to fullscreen";
      } else {
        document.title = "FULLSCREEN";
      }
    };
  } else {
    player.addEventListener('playing',
                            function() {
                              document.title = "PLAYING";
                            },
                            false);
  }
}

function StartPlayer(media_url) {
  player.src = media_url;
  player.play();
}
