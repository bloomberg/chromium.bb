// Copyright 2009 Google Inc.  All Rights Reserved.

/**
 * @fileoverview This file contains js that continues collect Popular video
 * from YouTube and displays them when needed.
 * @author lzheng@chromium.com (Lei Zheng)
 */

const YT_POLL_INTERVAL = 1000 * 600;  // 10 minute
var json_url_popular = 'http://gdata.youtube.com/feeds/api/standardfeeds/' +
    'most_popular?time=today&alt=json-in-script&format=5&max-results=50&' +
    'callback=processVideos';

// This holds all videos when browser starts.
var YT_existing_video_map = new Map();
// This holds all popular videos after browser starts.
var YT_latest_video_map = new Map();

var YT_reqCount = 0;

const YT_MAX_VIDEOS = 500;

var YT_start_time = Date();

/**
 * Class that holds video information that will be displayed.
 * @param {string} video_url Video Url from Gdata for a given video.
 * @param {string} thumb_url The url to access thumbnails.
 * @param {string} title Title of the video.
 * @param {string} description The description of the video.
 */
function videoEntry(video_url, thumb_url, title, description) {
  this.video_url = video_url;
  this.thumb_url = thumb_url;
  this.title = title;
  this.description = description;
  this.time = Date();
}

/**
 * Dump what's in the video_map to html.
 * @param {Map} video_map The Map that contains video information.
 * @return {string} The html output.
 */
function outputMap(video_map) {
  var keys = video_map.keys() || [];
  var output = '<table>';
  for (var i = 0; i < keys.length; ++i) {
    console.log(keys[i]);
    var title = video_map.get(keys[i]).title;
    var playerUrl = video_map.get(keys[i]).video_url;
    var thumbnailUrl = video_map.get(keys[i]).thumb_url;
    var time = video_map.get(keys[i]).time;
    var description = video_map.get(keys[i]).description;
    if (i % 3 == 0) {
      if (i != 0) {
        output += '</tr>';
      }
      output += '<tr>';
    }
    output += '<td><a href = \' ' + playerUrl + '\' target=\'_black\'\'>' +
        '<img src="'+ thumbnailUrl + '" width="130" height="97"/><br />' +
        '<span class="titlec">' + title + '...</span><br /></a><br />' +
        '<font size=2>' + description + '... </font></td>';
  }
  output += '</tr></table>';
  return output;
}

/**
 * Display popular videos when and after browser started.
 */
function showYTLatest() {
  var new_win = window.open();
  var html = ['<html><head><title>New Popular YouTube Videos</title><style>',
              '.titlec {  font-size: small;}ul.videos li {  float: left;  ',
              'width: 10em;  margin-bottom: 1em;}ul.videos{  ',
              'margin-bottom: 1em;  padding-left : 0em;  margin-left: 0em; ',
              'list-style: none;}</style></head><body>'];
  console.log('show my videos total: ' + YT_latest_video_map.size());
  html.push('<div id="videos"><font size="5" color="green">',
            YT_latest_video_map.size(),
            '</font> new popular videos after you start',
            ' your browser at: <font size="1">', YT_start_time,
            '</font><ul>');

  html.push(outputMap(YT_latest_video_map));
  html.push('</ul></div>');

  html.push('<div id="videos"><font size="5" color="green">',
            YT_existing_video_map.size(),
            '</font> popular videos when you start your browser at: ',
            '<font size="1">', YT_start_time, '</font><ul>');
  html.push(outputMap(YT_existing_video_map));
  html.push('</ul></div>');
  html.push('</body></html>');
  console.log('html: ' + html.join(''));
  new_win.document.write(html.join(''));
}

/**
 * Callback function for GDataJson and schedule the next gdata call.
 * Extract data from Gdata and store them if needed.
 * @param {Gdata} gdata Gdata from YouTube.
 */
function processVideos(gdata) {
  var feed = gdata.feed;
  var entries = feed.entry || [];
  console.log('process videos total: ' + entries.length
              + 'reqCount=' + YT_reqCount);
  var first_run = false;

  if (YT_existing_video_map.size() == 0) {
    first_run = true;
  }
  for (var i = 0; i < entries.length; i++) {
    var title = entries[i].title.$t.substr(0,20);
    var thumb_url = entries[i].media$group.media$thumbnail[0].url;
    var video_url = entries[i].media$group.media$player[0].url;
    var description =
        entries[i].media$group.media$description.$t.substr(0, 100);
    if (first_run == true) {
      YT_existing_video_map.insert(
          title,
          new videoEntry(video_url, thumb_url, title, description));
      console.log(title);
    } else {
      if (YT_existing_video_map.get(title) == null) {
        // This is new, keep it
        if (YT_latest_video_map.size() < YT_MAX_VIDEOS) {
          YT_latest_video_map.insert(
              title,
              new videoEntry(video_url, thumb_url, title, description));
        } else {
          console.log('Too many new videos');
        }
      }
    }
  }

  // Schedule the next call
  document.getElementById('newCount').innerHTML = '(was ' +
      YT_existing_video_map.size() + ', new ' +
      YT_latest_video_map.size() + ')';
  var gdata_json = document.getElementById('gdata_json');

  // cleanup the json script inserted in fetchJSON
  document.getElementsByTagName('body')[0].removeChild(gdata_json);
  scheduleRequest();
}

/**
 * Fetch gdata via JSON.
 */
function fetchJSON() {
  var scr = document.createElement('script');
  scr.setAttribute('language', 'JavaScript');
  scr.setAttribute('src', json_url_popular + '&reqCount=' + YT_reqCount);
  scr.setAttribute('id', 'gdata_json');
  ++YT_reqCount;
  document.getElementsByTagName('body')[0].appendChild(scr);
}

/**
 * Where the extension starts.
 */
function start() {
  window.setTimeout(fetchJSON, 0);
}

/**
 * Schedule the fetchJSON call.
 */
function scheduleRequest() {
  window.setTimeout(fetchJSON, YT_POLL_INTERVAL);
}
