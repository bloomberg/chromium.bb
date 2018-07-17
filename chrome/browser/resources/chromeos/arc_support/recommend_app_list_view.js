// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var selectedApps = /** @dict */ {};

function generateContents(appIcon, appTitle, appPackageName) {
  var doc = document;
  var recommendAppsContainer = doc.getElementById('recommend-apps-container');
  var item = doc.createElement('div');
  item.classList.add('item');
  item.classList.add('checked');
  item.setAttribute('data-packagename', appPackageName);
  item.onclick = toggleCheckStatus;

  var imagePicker = doc.createElement('div');
  imagePicker.classList.add('image-picker');
  item.appendChild(imagePicker);

  var chip = doc.createElement('div');
  chip.classList.add('chip');
  item.appendChild(chip);

  var img = doc.createElement('img');
  img.classList.add('app-icon');
  img.setAttribute('src', decodeURIComponent(appIcon));

  var title = doc.createElement('span');
  title.classList.add('app-title');
  title.innerHTML = appTitle;

  chip.appendChild(img);
  chip.appendChild(title);

  recommendAppsContainer.appendChild(item);
}

// Toggle the check status of an app. If an app is selected, add the "checked"
// class so that the checkmark is visible. Otherwise, remove the checked class.
function toggleCheckStatus(e) {
  var item = e.currentTarget;
  item.classList.toggle('checked');
}

function getSelectedPackages() {
  var selectedPackages = [];
  var checkedItems = document.getElementsByClassName('checked');
  for (var checkedItem of checkedItems) {
    selectedPackages.push(checkedItem.dataset.packagename);
  }
  return selectedPackages;
}