// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Window initialization code. Set up event handlers.
window.addEventListener("load", function() {
  // set up the event listeners.
  chrome.notificationProvider.onCreated.addListener(notificationCreated);
  chrome.notificationProvider.onUpdated.addListener(notificationUpdated);
  chrome.notificationProvider.onCleared.addListener(notificationCleared);
});

function displayImage(text, bitmap, div) {
  var image = document.createElement("p");
  image.appendChild(document.createTextNode(text));
  var imageCanvas = document.createElement("canvas");
  image.appendChild(imageCanvas);
  div.appendChild(image);

  var imageContext = imageCanvas.getContext('2d');
  imageCanvas.width = bitmap.width;
  imageCanvas.height = bitmap.height;
  var imagedata = imageContext.createImageData(bitmap.width,
                                              bitmap.height);
  var dataView = new Uint8Array(bitmap.data);
  for (var i = 0; i < bitmap.width * bitmap.height * 4; i += 1) {
    imagedata.data[i] = dataView[i];
  }
  imageContext.putImageData(imagedata, 0, 0);
}

function addNotification(senderId, notificationId, details) {
  var list = document.getElementById("notification_list");

  var div = document.createElement("div");
  div.class = "notifications";
  div.id = senderId + notificationId;

  line = document.createElement("br");
  div.appendChild(line);

  // Create a close button.
  var closeButton = document.createElement("button");
  closeButton.class = "closeButtons";
  var buttonText = document.createTextNode("close notificaiton");
  closeButton.appendChild(buttonText);
  div.appendChild(closeButton);
  closeButton.addEventListener("click", function(){
      clearNotification(senderId, notificationId)
  }, false);

  // Display title.
  var bold = document.createElement("b");
  var title = document.createElement("p");
  bold.appendChild(document.createTextNode(details.title));
  title.appendChild(bold);
  div.appendChild(title);

  // Display notification ID.
  var id = document.createElement("p");
  id.appendChild(document.createTextNode("Notification ID: " + notificationId));
  div.appendChild(id);

  // Disply the type of notication.
  var notType = document.createElement("p");
  notType.appendChild(document.createTextNode("Type: " + details.type));
  div.appendChild(notType);

  // Display the priority field of the notification.
  var priority = document.createElement("p");
  priority.appendChild(document.createTextNode("Priority: " +
                                               details.priority));
  div.appendChild(priority);

  // Display the message of the notification.
  var message = document.createElement("p");
  message.appendChild(document.createTextNode("Message: " + details.message));
  div.appendChild(message);

  // Display the icon image of the notification.
  displayImage("Icon: ", details.iconBitmap, div);

  // Display the context message of the notification if it has one.
  if ("contextMessage" in details) {
    var message = document.createElement("p");
    message.appendChild(document.createTextNode("Message: " + details.message));
    div.appendChild(message);
  }

  // Display the progress of the notification if it has one.
  if (details.type == "progress" && "progress" in details) {
    var progress = document.createElement("p");
    progress.appendChild(document.createTextNode("Progress: " +
                                                 details.progress));
    div.appendChild(progress);
  }

  // Display if the notification is clickable.
  if ("isClickable" in details) {
    var clickable = document.createElement("p");
    clickable.appendChild(document.createTextNode("IsClickable: " +
                                                  details.isClickable));
    div.appendChild(clickable);
  }

  // Display the time the notification was created.
  if ("eventTime" in details) {
    var time = document.createElement("p");
    time.appendChild(document.createTextNode("Event Time: " +
                                             details.eventTime));
    div.appendChild(time);
  }

  // Display the list data of the notification if it's of list type.
  if (details.type = "list" && "items" in details) {
    for (var i = 0, size = details.items.length; i < size; i++) {
      var item = document.createElement("p");
      item.appendChild(document.createTextNode(
                                "Item " + (i+1) + ": " +
                                details.items[i].title + " - " +
                                details.items[i].message));
      div.appendChild(item);
    }
  }

  // Display image of the notification if it's of image type.
  if (details.type = "image" && "imageBitmap" in details) {
    displayImage("Image: ", details.imageBitmap, div);
  }

  // Display the buttons of the notification if it has some.
  if ("buttons" in details) {
    for (var i = 0, size = details.buttons.length; i < size; i++) {
      var button = document.createElement("p");
      button.appendChild(document.createTextNode(
                  "Button " + (i+1) + ": " + details.buttons[i].title));
      div.appendChild(button);
      if ("iconBitmap" in details.buttons[i]) {
        displayImage("Button " + (i+1) + ": ",
                     details.buttons[i].iconBitmap,
                     div);
      }
    }
  }

  div.appendChild(document.createElement("br"));
  list.appendChild(div);
}

function clearedCallback(ifCleared){}

function clearNotification(senderId, notificationId) {
  var list = document.getElementById("notification_list");
  list.removeChild(document.getElementById(senderId + notificationId));
  chrome.notificationProvider.notifyOnCleared(senderId,
                                              notificationId,
                                              clearedCallback);
}

function notificationCreated(senderId, notificationId, details){
  addNotification(senderId, notificationId, details);
}

function notificationUpdated(senderId, notificationId, details){
  var list = document.getElementById("notification_list");
  list.removeChild(document.getElementById(senderId + notificationId));
  addNotification(senderId, notificationId, details);
}

function notificationCleared(senderId, notificationId){
  clearNotification(senderId, notificationId);
}
