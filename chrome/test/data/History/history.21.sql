BEGIN TRANSACTION;
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,value LONGVARCHAR);
INSERT INTO "meta" VALUES('version','21');
INSERT INTO "meta" VALUES('last_compatible_version','16');
CREATE TABLE android_urls(id INTEGER PRIMARY KEY, raw_url LONGVARCHAR, created_time INTEGER NOT NULL, last_visit_time INTEGER NOT NULL, url_id INTEGER NOT NULL,favicon_id INTEGER DEFAULT NULL, bookmark INTEGER DEFAULT 0);
CREATE INDEX android_urls_raw_url_idx ON android_urls(raw_url);
CREATE INDEX android_urls_url_id_idx ON android_urls(url_id);
INSERT INTO "android_urls" VALUES(1,'http://google.com/',1,1,1,1,0);
INSERT INTO "android_urls" VALUES(4,'www.google.com/',1,1,3,1,1);
COMMIT;


