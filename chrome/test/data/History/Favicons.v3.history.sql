-- unit_tests --gtest_filter=ThumbnailDatabaseTest.Version3
--
-- .dump that portion of the History database needed to migrate a
-- Favicons version 3 database.  See also Favicons.v3.sql.
BEGIN TRANSACTION;
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,value LONGVARCHAR);
INSERT INTO "meta" VALUES('version','20');
INSERT INTO "meta" VALUES('last_compatible_version','16');
CREATE TABLE urls(id INTEGER PRIMARY KEY,url LONGVARCHAR,title LONGVARCHAR,visit_count INTEGER DEFAULT 0 NOT NULL,typed_count INTEGER DEFAULT 0 NOT NULL,last_visit_time INTEGER NOT NULL,hidden INTEGER DEFAULT 0 NOT NULL,favicon_id INTEGER DEFAULT 0 NOT NULL);
INSERT INTO "urls" VALUES(1,'http://google.com/','Google',1,1,0,0,1);
INSERT INTO "urls" VALUES(2,'http://www.google.com/','Google',1,0,0,0,1);
INSERT INTO "urls" VALUES(3,'http://www.google.com/blank.html','',1,0,0,1,0);
INSERT INTO "urls" VALUES(4,'http://yahoo.com/','Yahoo!',1,1,0,0,2);
INSERT INTO "urls" VALUES(5,'http://www.yahoo.com/','Yahoo!',1,0,0,0,2);
COMMIT;
