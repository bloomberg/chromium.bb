// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.setTestOnly();
goog.require('mr.Route');
goog.require('mr.mirror.Session');

describe('Tests mr.mirror.Session', function() {

  let mirrorRoute;
  let session;

  beforeEach(function() {
    mirrorRoute = new mr.Route(
        'routeId', 'presentationId', 'sinkId', 'tab:47', true, null);
    session = new mr.mirror.Session(mirrorRoute);
    spyOn(session, 'sendMetadataToSinkInternal');
    spyOn(session, 'onRouteUpdated');
  });

  it('has default data', function() {
    expect(session.route).toBe(mirrorRoute);
    expect(session.isIncognito).toBe(false);
    expect(session.iconUrl).toBeNull();
    expect(session.localTitle).toBeNull();
    expect(session.remoteTitle).toBeNull();
  });

  describe('setMetadata', function() {
    it('sets all fields with normal tab', function() {
      session.setMetadata(
          'Google News', 'news.google.com (Tab)',
          'http://news.google.com/icon.png');
      expect(session.iconUrl).toBe('http://news.google.com/icon.png');
      expect(session.localTitle).toBe('Google News');
      expect(session.remoteTitle).toBe('news.google.com (Tab)');
    });
    it('sets some fields with incognito tab', function() {
      session.isIncognito = true;
      session.setMetadata(
          'Google News', 'news.google.com (Tab)',
          'http://news.google.com/icon.png');
      expect(session.iconUrl).toBeNull();
      expect(session.localTitle).toBe('Google News');
      expect(session.remoteTitle).toBeNull();
    });
    it('sets some fields with OTR route', function() {
      mirrorRoute.offTheRecord = true;
      session.setMetadata(
          'Google News', 'news.google.com (Tab)',
          'http://news.google.com/icon.png');
      expect(session.iconUrl).toBeNull();
      expect(session.localTitle).toBe('Google News');
      expect(session.remoteTitle).toBeNull();
    });
    it('is called with tab info', function() {
      session.updateTabInfo({
        incognito: false,
        title: 'Google News',
        url: 'http://news.google.com/',
        favIconUrl: 'http://news.google.com/icon.png'
      });
      expect(session.isIncognito).toBe(false);
      expect(session.iconUrl).toBe('http://news.google.com/icon.png');
      expect(session.localTitle).toBe('Google News');
      expect(session.remoteTitle).toBe('news.google.com (Tab)');
      expect(session.onRouteUpdated).toHaveBeenCalled();
    });
  });

  describe('sendMetadataToSink', function() {
    it('sends metadata for normal tab', function() {
      session.sendMetadataToSink();
      expect(session.sendMetadataToSinkInternal).toHaveBeenCalled();
    });
    it('does not send metadata for incognito tab', function() {
      session.isIncognito = true;
      session.sendMetadataToSink();
      expect(session.sendMetadataToSinkInternal).not.toHaveBeenCalled();
    });
    it('does not send metadata for OTR route', function() {
      mirrorRoute.offTheRecord = true;
      session.sendMetadataToSink();
      expect(session.sendMetadataToSinkInternal).not.toHaveBeenCalled();
    });
  });
});
