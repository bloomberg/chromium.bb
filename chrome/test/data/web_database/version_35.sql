PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,value LONGVARCHAR);
INSERT INTO "meta" VALUES('version','35');
INSERT INTO "meta" VALUES('last_compatible_version','35');
INSERT INTO "meta" VALUES('Default Search Provider ID','2');
INSERT INTO "meta" VALUES('Builtin Keyword Version','33');
CREATE TABLE keywords (id INTEGER PRIMARY KEY,short_name VARCHAR NOT NULL,keyword VARCHAR NOT NULL,favicon_url VARCHAR NOT NULL,url VARCHAR NOT NULL,show_in_default_list INTEGER,safe_for_autoreplace INTEGER,originating_url VARCHAR,date_created INTEGER DEFAULT 0,usage_count INTEGER DEFAULT 0,input_encodings VARCHAR,suggest_url VARCHAR,prepopulate_id INTEGER DEFAULT 0,autogenerate_keyword INTEGER DEFAULT 0,logo_id INTEGER DEFAULT 0,created_by_policy INTEGER DEFAULT 0,instant_url VARCHAR);
INSERT INTO "keywords" VALUES(2,'Google','google.com','http://www.google.com/favicon.ico','{google:baseURL}search?{google:RLZ}{google:acceptedSuggestion}{google:originalQueryForSuggestion}sourceid=chrome&ie={inputEncoding}&q={searchTerms}',1,1,'',0,0,'UTF-8','{google:baseSuggestURL}search?client=chrome&hl={language}&q={searchTerms}',1,1,6262,0,'{google:baseURL}webhp?{google:RLZ}sourceid=chrome-instant&ie={inputEncoding}&ion=1{searchTerms}&nord=1');
INSERT INTO "keywords" VALUES(3,'Yahoo!','yahoo.com','http://search.yahoo.com/favicon.ico','http://search.yahoo.com/search?ei={inputEncoding}&fr=crmas&p={searchTerms}',1,1,'',0,0,'UTF-8','http://ff.search.yahoo.com/gossip?output=fxjson&command={searchTerms}',2,0,6279,0,'');
INSERT INTO "keywords" VALUES(4,'Bing','bing.com','http://www.bing.com/s/wlflag.ico','http://www.bing.com/search?setmkt=en-US&q={searchTerms}',1,1,'',0,0,'UTF-8','http://api.bing.com/osjson.aspx?query={searchTerms}&language={language}',3,0,6256,0,'');
CREATE TABLE logins (origin_url VARCHAR NOT NULL, action_url VARCHAR, username_element VARCHAR, username_value VARCHAR, password_element VARCHAR, password_value BLOB, submit_element VARCHAR, signon_realm VARCHAR NOT NULL,ssl_valid INTEGER NOT NULL,preferred INTEGER NOT NULL,date_created INTEGER NOT NULL,blacklisted_by_user INTEGER NOT NULL,scheme INTEGER NOT NULL,UNIQUE (origin_url, username_element, username_value, password_element, submit_element, signon_realm));
CREATE TABLE web_app_icons (url LONGVARCHAR,width int,height int,image BLOB, UNIQUE (url, width, height));
CREATE TABLE web_apps (url LONGVARCHAR UNIQUE,has_all_images INTEGER NOT NULL);
CREATE TABLE autofill (name VARCHAR, value VARCHAR, value_lower VARCHAR, pair_id INTEGER PRIMARY KEY, count INTEGER DEFAULT 1);
CREATE TABLE autofill_dates ( pair_id INTEGER DEFAULT 0, date_created INTEGER DEFAULT 0);
CREATE TABLE autofill_profiles ( guid VARCHAR PRIMARY KEY, company_name VARCHAR, address_line_1 VARCHAR, address_line_2 VARCHAR, city VARCHAR, state VARCHAR, zipcode VARCHAR, country VARCHAR, country_code VARCHAR, date_modified INTEGER NOT NULL DEFAULT 0);
CREATE TABLE autofill_profile_names ( guid VARCHAR, first_name VARCHAR, middle_name VARCHAR, last_name VARCHAR);
CREATE TABLE autofill_profile_emails ( guid VARCHAR, email VARCHAR);
CREATE TABLE autofill_profile_phones ( guid VARCHAR, type INTEGER DEFAULT 0, number VARCHAR);

/* A "John Doe" profile with all-valid fields. */
INSERT INTO "autofill_profiles" VALUES('00000000-0000-0000-0000-000000000001','Acme Inc.','1 Main Street','Apt 2','San Francisco','CA','94102','United States','US',1300131704);
INSERT INTO "autofill_profile_names" VALUES('00000000-0000-0000-0000-000000000001','John','','Doe');
INSERT INTO "autofill_profile_emails" VALUES('00000000-0000-0000-0000-000000000001','john@doe.com');
INSERT INTO "autofill_profile_phones" VALUES('00000000-0000-0000-0000-000000000001',0,'4151112222');
INSERT INTO "autofill_profile_phones" VALUES('00000000-0000-0000-0000-000000000001',1,'4151110000');

/* A subset of "John Doe".  Should get discarded. */
INSERT INTO "autofill_profiles" VALUES('00000000-0000-0000-0000-000000000002','','1 Main Street','Apt 2','San Francisco','CA','94102','United States','US',1300131704);
INSERT INTO "autofill_profile_names" VALUES('00000000-0000-0000-0000-000000000002','John','','Doe');
INSERT INTO "autofill_profile_emails" VALUES('00000000-0000-0000-0000-000000000002','john@doe.com');
INSERT INTO "autofill_profile_phones" VALUES('00000000-0000-0000-0000-000000000002',0,'4151112222');
INSERT INTO "autofill_profile_phones" VALUES('00000000-0000-0000-0000-000000000002',1,'4151110000');

/* A profile with incomplete address.  Should get discarded. */
INSERT INTO "autofill_profiles" VALUES('00000000-0000-0000-0000-000000000003','','','Apt 3','San Francisco','CA','94102','United States','US',1300131704);
INSERT INTO "autofill_profile_names" VALUES('00000000-0000-0000-0000-000000000003','Jim','','Smith');

/* A profile with bad email. Should get discarded. */
INSERT INTO "autofill_profiles" VALUES('00000000-0000-0000-0000-000000000004','Acme Inc.','4 Main Street','Apt 2','San Francisco','CA','94102','United States','US',1300131704);
INSERT INTO "autofill_profile_emails" VALUES('00000000-0000-0000-0000-000000000004','bademail');

/* A profile with bad State (country == US).  Should get discarded. */
INSERT INTO "autofill_profiles" VALUES('00000000-0000-0000-0000-000000000005','Acme Inc.','6 Main Street','Apt 2','San Francisco','BS','94102','United States','US',1300131704);
INSERT INTO "autofill_profile_names" VALUES('00000000-0000-0000-0000-000000000006','John','','Doe');

/* A profile with bad zip (country == US).  Should get discarded. */
INSERT INTO "autofill_profiles" VALUES('00000000-0000-0000-0000-000000000006','Acme Inc.','7 Main Street','Apt 2','San Francisco','CA','bogus','United States','US',1300131704);
INSERT INTO "autofill_profile_names" VALUES('00000000-0000-0000-0000-000000000007','John','','Doe');

CREATE TABLE credit_cards ( guid VARCHAR PRIMARY KEY, name_on_card VARCHAR, expiration_month INTEGER, expiration_year INTEGER, card_number_encrypted BLOB, date_modified INTEGER NOT NULL DEFAULT 0);
CREATE TABLE token_service (service VARCHAR PRIMARY KEY NOT NULL,encrypted_token BLOB);
CREATE INDEX logins_signon ON logins (signon_realm);
CREATE INDEX web_apps_url_index ON web_apps (url);
CREATE INDEX autofill_name ON autofill (name);
CREATE INDEX autofill_name_value_lower ON autofill (name, value_lower);
CREATE INDEX autofill_dates_pair_id ON autofill_dates (pair_id);
COMMIT;
