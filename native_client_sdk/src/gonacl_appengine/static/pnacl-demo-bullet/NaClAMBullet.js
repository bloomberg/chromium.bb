// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function NaClAMBulletInit() {
  aM.addEventListener('sceneloaded', NaClAMBulletSceneLoadedHandler);
  aM.addEventListener('noscene', NaClAMBulletStepSceneHandler);
  aM.addEventListener('sceneupdate', NaClAMBulletStepSceneHandler);
}

function NaClAMBulletLoadScene(sceneDescription) {
  aM.sendMessage('loadscene', sceneDescription);
}

function NaClAMBulletSceneLoadedHandler(msg) {
  console.log('Scene loaded.');
  console.log('Scene object count = ' + msg.header.sceneobjectcount);
}

function NaClAMBulletPickObject(objectTableIndex, cameraPos, hitPos) {
  aM.sendMessage('pickobject', {index: objectTableIndex, cpos: [cameraPos.x, cameraPos.y, cameraPos.z], pos: [hitPos.x,hitPos.y,hitPos.z]});
}

function NaClAMBulletDropObject() {
  aM.sendMessage('dropobject', {});
}

function NaClAMBulletStepSceneHandler(msg) {
  // Step the scene
  var i;
  var j;
  var numTransforms = 0;
  if (msg.header.cmd == 'sceneupdate') {
    if (skipSceneUpdates > 0) {
      skipSceneUpdates--;
      return;
    }
    var simTime = msg.header.simtime;
    document.getElementById('simulationTime').textContent = simTime;
    TransformBuffer = new Float32Array(msg.frames[0]);
    numTransforms = TransformBuffer.length/16;
    for (i = 0; i < numTransforms; i++) {
      for (j = 0; j < 16; j++) {
        objects[i].matrixWorld.elements[j] = TransformBuffer[i*16+j];
      }
    }
  }
}
