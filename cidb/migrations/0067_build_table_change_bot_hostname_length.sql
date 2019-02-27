ALTER TABLE buildTable MODIFY COLUMN bot_hostname VARCHAR(160) NOT NULL;

INSERT INTO schemaVersionTable (schemaVersion, scriptName) VALUES
  (67, '0067_build_table_change_bot_hostname_length.sql');
