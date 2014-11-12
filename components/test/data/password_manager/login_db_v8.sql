PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);
INSERT INTO "meta" VALUES('last_compatible_version','1');
INSERT INTO "meta" VALUES('version','8');
CREATE TABLE logins (origin_url VARCHAR NOT NULL, action_url VARCHAR, username_element VARCHAR, username_value VARCHAR, password_element VARCHAR, password_value BLOB, submit_element VARCHAR, signon_realm VARCHAR NOT NULL,ssl_valid INTEGER NOT NULL,preferred INTEGER NOT NULL,date_created INTEGER NOT NULL,blacklisted_by_user INTEGER NOT NULL,scheme INTEGER NOT NULL,password_type INTEGER,possible_usernames BLOB,times_used INTEGER,form_data BLOB,use_additional_auth INTEGER,date_synced INTEGER, display_name VARCHAR, avatar_url VARCHAR, federation_url VARCHAR, is_zero_click INTEGER,UNIQUE (origin_url, username_element, username_value, password_element, submit_element, signon_realm));
INSERT INTO "logins" VALUES('https://accounts.google.com/ServiceLogin','https://accounts.google.com/ServiceLoginAuth','Email','theerikchen','Passwd',X'','','https://accounts.google.com/',1,1,1402955745,0,0,0,X'00000000',1,X'18000000020000000000000000000000000000000000000000000000',NULL,0,'','','',0);
INSERT INTO "logins" VALUES('https://accounts.google.com/ServiceLogin','https://accounts.google.com/ServiceLoginAuth','Email','theerikchen2','Passwd',X'','','https://accounts.google.com/',1,1,1402950000,0,0,0,X'00000000',1,X'18000000020000000000000000000000000000000000000000000000',NULL,0,'','','',0);
CREATE INDEX logins_signon ON logins (signon_realm);
COMMIT;

