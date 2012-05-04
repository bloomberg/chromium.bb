PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,value LONGVARCHAR);
INSERT INTO "meta" VALUES('version','44');
INSERT INTO "meta" VALUES('last_compatible_version','44');
INSERT INTO "meta" VALUES('Default Search Provider ID Backup','0');
INSERT INTO "meta" VALUES('Default Search Provider ID Backup Signature','ÚG"2qÐÀgõðß!ƒë¸×Ÿ‹gXÑiõg¢ÑsJw');
CREATE TABLE keywords (id INTEGER PRIMARY KEY,short_name VARCHAR NOT NULL,keyword VARCHAR NOT NULL,favicon_url VARCHAR NOT NULL,url VARCHAR NOT NULL,show_in_default_list INTEGER,safe_for_autoreplace INTEGER,originating_url VARCHAR,date_created INTEGER DEFAULT 0,usage_count INTEGER DEFAULT 0,input_encodings VARCHAR,suggest_url VARCHAR,prepopulate_id INTEGER DEFAULT 0,autogenerate_keyword INTEGER DEFAULT 0,logo_id INTEGER DEFAULT 0,created_by_policy INTEGER DEFAULT 0,instant_url VARCHAR,last_modified INTEGER DEFAULT 0,sync_guid VARCHAR);
CREATE TABLE keywords_backup(
  id INT,
  short_name TEXT,
  keyword TEXT,
  favicon_url TEXT,
  url TEXT,
  safe_for_autoreplace INT,
  originating_url TEXT,
  date_created INT,
  usage_count INT,
  input_encodings TEXT,
  show_in_default_list INT,
  suggest_url TEXT,
  prepopulate_id INT,
  autogenerate_keyword INT,
  logo_id INT,
  created_by_policy INT,
  instant_url TEXT,
  last_modified INT,
  sync_guid TEXT
);
CREATE TABLE autofill (name VARCHAR, value VARCHAR, value_lower VARCHAR, pair_id INTEGER PRIMARY KEY, count INTEGER DEFAULT 1);
CREATE TABLE credit_cards ( guid VARCHAR PRIMARY KEY, name_on_card VARCHAR, expiration_month INTEGER, expiration_year INTEGER, card_number_encrypted BLOB, date_modified INTEGER NOT NULL DEFAULT 0);
CREATE TABLE autofill_dates ( pair_id INTEGER DEFAULT 0, date_created INTEGER DEFAULT 0);
CREATE TABLE autofill_profiles ( guid VARCHAR PRIMARY KEY, company_name VARCHAR, address_line_1 VARCHAR, address_line_2 VARCHAR, city VARCHAR, state VARCHAR, zipcode VARCHAR, country VARCHAR, country_code VARCHAR, date_modified INTEGER NOT NULL DEFAULT 0);
CREATE TABLE autofill_profile_names ( guid VARCHAR, first_name VARCHAR, middle_name VARCHAR, last_name VARCHAR);
CREATE TABLE autofill_profile_emails ( guid VARCHAR, email VARCHAR);
CREATE TABLE autofill_profile_phones ( guid VARCHAR, type INTEGER DEFAULT 0, number VARCHAR);
CREATE TABLE autofill_profiles_trash ( guid VARCHAR);
CREATE TABLE logins (origin_url VARCHAR NOT NULL, action_url VARCHAR, username_element VARCHAR, username_value VARCHAR, password_element VARCHAR, password_value BLOB, submit_element VARCHAR, signon_realm VARCHAR NOT NULL,ssl_valid INTEGER NOT NULL,preferred INTEGER NOT NULL,date_created INTEGER NOT NULL,blacklisted_by_user INTEGER NOT NULL,scheme INTEGER NOT NULL,UNIQUE (origin_url, username_element, username_value, password_element, submit_element, signon_realm));
CREATE TABLE ie7_logins (url_hash VARCHAR NOT NULL, password_value BLOB, date_created INTEGER NOT NULL,UNIQUE (url_hash));
CREATE TABLE web_app_icons (url LONGVARCHAR,width int,height int,image BLOB, UNIQUE (url, width, height));
CREATE TABLE web_apps (url LONGVARCHAR UNIQUE,has_all_images INTEGER NOT NULL);
CREATE TABLE token_service (service VARCHAR PRIMARY KEY NOT NULL,encrypted_token BLOB);
CREATE TABLE web_intents (service_url LONGVARCHAR,action VARCHAR,type VARCHAR,title LONGVARCHAR,disposition VARCHAR,UNIQUE (service_url, action, type));
CREATE TABLE web_intents_defaults (action VARCHAR,type VARCHAR,url_pattern LONGVARCHAR,user_date INTEGER,suppression INTEGER,service_url LONGVARCHAR,UNIQUE (action, type, url_pattern));
CREATE INDEX autofill_name ON autofill (name);
CREATE INDEX autofill_name_value_lower ON autofill (name, value_lower);
CREATE INDEX autofill_dates_pair_id ON autofill_dates (pair_id);
CREATE INDEX logins_signon ON logins (signon_realm);
CREATE INDEX ie7_logins_hash ON ie7_logins (url_hash);
CREATE INDEX web_apps_url_index ON web_apps (url);
CREATE INDEX web_intents_index ON web_intents (action);
CREATE INDEX web_intents_default_index ON web_intents_defaults (action);
COMMIT;
